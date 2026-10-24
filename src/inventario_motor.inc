// Adaptación de Trinity (MIT), revisión 70c9a00dd6e10b2081d706a837756844c11f5c2b.
// Las llamadas al motor ocurren en el hilo que entra al hook, nunca desde el menú.

#include "inventario_puente.hpp"
#include "inventario_motor.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <mutex>
#include <vector>
#include <algorithm>

#include <MinHook.h>

namespace trinity::game
{
    using mem::Read64;
    using mem::Read32;
    using mem::Read16;
    using mem::Read8;
    using mem::ReadPtr;
    using mem::Write64;
    using mem::Write16;
    using mem::Write8;
    using mem::ReadCString;

    namespace
    {
        
        using GetItemQty_t = int64_t(__fastcall*)(void* container, uint16_t typeId, void* keyPtr);
        using GetHolder_t  = void*(__fastcall*)(void* container);

        using HolderInsert_t = void*(__fastcall*)(void*, void*, void*, void*, uint16_t,
                                                  void*, uint8_t, uint8_t, uint8_t);
        
        using Commit_t = void*(__fastcall*)(void*, void*, void*, void*, void*, uint8_t, uint8_t);

        using SetExpandSlots_t = void*(__fastcall*)(void*, int*, void*, uint16_t, uint16_t);

        using ItemValueCtor_t   = void*(__fastcall*)(void* itemVal, uint16_t* typeId, int64_t qty);
        using CommitPlacement_t = void*(__fastcall*)(void* holder, int* err, void* unused,
                                                     void* placement, uint16_t slotIdx);
        using FreePlacements_t  = void(__fastcall*)(void* vec);
        using ItemValueDtor_t   = void(__fastcall*)(void* itemVal);
        GetItemQty_t      oGetItemQty      = nullptr;
        GetHolder_t       oGetHolder       = nullptr;
        HolderInsert_t    oHolderInsert    = nullptr;
        Commit_t          oCommit          = nullptr;
        SetExpandSlots_t  oSetExpandSlots  = nullptr;
        ItemValueCtor_t   oItemValueCtor   = nullptr;
        CommitPlacement_t oCommitPlacement = nullptr;
        FreePlacements_t  oFreePlacements  = nullptr;
        ItemValueDtor_t   oItemValueDtor   = nullptr;
        void*          g_qtyTarget   = nullptr;
        void*          g_insTarget   = nullptr;
        void*          g_commitTarget = nullptr;
        void*          g_expandTarget = nullptr;

        std::atomic<uintptr_t> g_holder{0};
        std::atomic<ULONGLONG> g_holderTick{0};

        struct Candidate { uintptr_t container; uintptr_t holder; ULONGLONG tick; };
        constexpr int       kMaxCandidates = 16;
        constexpr ULONGLONG kCandGraceMs   = 10000; 
        Candidate            g_cand[kMaxCandidates] = {};
        std::atomic<int>     g_candCount{0};
        CRITICAL_SECTION     g_candLock;
        bool                 g_candLockInit = false;

        std::atomic<uintptr_t> g_serverHolder{0};
        std::atomic<uintptr_t> g_serverContainer{0};
        std::atomic<ULONGLONG> g_serverTick{0};

        uintptr_t g_coreGlobal = 0;

        uintptr_t g_itemTableGlobal = 0;

        uintptr_t g_grpTableGlobal = 0;
        uintptr_t g_strTableGlobal = 0;
        uintptr_t g_invTableGlobal = 0;
        uintptr_t g_locMgrGlobal   = 0;

        std::vector<int64_t> g_origMaxStack;
        std::vector<uint8_t> g_origApplyCap;
        std::vector<bool>    g_stackCaptured;

        struct BucketCap { uintptr_t bucket; uint16_t type; uint16_t origExpand; };
        std::vector<BucketCap> g_origBucketCap;

        CRITICAL_SECTION g_capLock;
        bool             g_capLockInit = false;

        bool FindOrigBucketCap(uintptr_t bucket, uint16_t type, uint16_t* out)
        {
            if (!g_capLockInit) return false;
            EnterCriticalSection(&g_capLock);
            bool found = false;
            for (const auto& e : g_origBucketCap)
                if (e.bucket == bucket && e.type == type) { *out = e.origExpand; found = true; break; }
            LeaveCriticalSection(&g_capLock);
            return found;
        }

        void CaptureOrigExpandOnce(uintptr_t bucket, uint16_t type)
        {
            if (!g_capLockInit) return;
            EnterCriticalSection(&g_capLock);
            bool have = false;
            for (const auto& e : g_origBucketCap)
                if (e.bucket == bucket && e.type == type) { have = true; break; }
            uint16_t orig = 0;
            if (!have && Read16(bucket + kOff_InvBucket_ExpandSlots, &orig))
                g_origBucketCap.push_back({ bucket, type, orig });
            LeaveCriticalSection(&g_capLock);
        }

        void UpsertOrigExpand(uintptr_t bucket, uint16_t type, uint16_t vanilla)
        {
            if (!g_capLockInit) return;
            EnterCriticalSection(&g_capLock);
            bool have = false;
            for (auto& e : g_origBucketCap)
                if (e.bucket == bucket && e.type == type) { e.origExpand = vanilla; have = true; break; }
            if (!have)
                g_origBucketCap.push_back({ bucket, type, vanilla });
            LeaveCriticalSection(&g_capLock);
        }

        bool    g_stackApplied  = false;
        int64_t g_stackAppliedVal = 0;
        bool    g_slotApplied     = false;
        int     g_slotAppliedVal  = 0;

        bool DefForRow(uintptr_t tableGlobal, uint16_t row, uintptr_t* out)
        {
            if (!tableGlobal) return false;
            uintptr_t table = 0;
            if (!ReadPtr(tableGlobal, &table)) return false;
            uint32_t count = 0;
            if (!Read32(table + kOff_ItemTable_Count, &count) || row >= count) return false;
            uintptr_t defs = 0;
            if (!ReadPtr(table + kOff_ItemTable_Defs, &defs)) return false;
            uintptr_t def = 0;
            if (!ReadPtr(defs + static_cast<uintptr_t>(row) * 8, &def)) return false;
            if (def < kMinPointer) return false;
            *out = def;
            return true;
        }

        bool StringField(uintptr_t fieldAddr, char* out, size_t n)
        {
            uintptr_t strObj = 0;
            if (!ReadPtr(fieldAddr, &strObj)) return false;
            uintptr_t buf = 0;
            if (!ReadPtr(strObj, &buf)) return false; 
            return ReadCString(buf, out, n);
        }

        bool KeyForType(uint16_t typeId, char* out, size_t n)
        {
            uintptr_t def = 0;
            if (!DefForRow(g_itemTableGlobal, typeId, &def)) return false;
            return StringField(def + kOff_ItemDef_Key, out, n);
        }

        bool LocString(uintptr_t structAddr, char* out, size_t n)
        {
            if (!g_locMgrGlobal) return false;
            uintptr_t provider = 0;
            if (!ReadPtr(structAddr, &provider) || provider < kMinPointer) return false;
            uint32_t off = 0;
            if (!Read32(provider + kOff_LocProv_Offset, &off)) return false;
            uintptr_t mgr = 0, blob = 0, data = 0;
            uint32_t  size = 0;
            if (!ReadPtr(g_locMgrGlobal, &mgr) || mgr < kMinPointer) return false;
            if (!ReadPtr(mgr + kOff_LocMgr_Blob, &blob) || blob < kMinPointer) return false;
            if (!Read32(blob + kOff_LocBlob_Size, &size) || off >= size) return false; 
            if (!ReadPtr(blob + kOff_LocBlob_Data, &data) || data < kMinPointer) return false;
            if (!ReadCString(data + off, out, n)) return false;
            return out[0] != 0;
        }

        bool DisplayNameForType(uint16_t typeId, char* out, size_t n)
        {
            uintptr_t def = 0;
            if (!DefForRow(g_itemTableGlobal, typeId, &def)) return false;
            return LocString(def + kOff_ItemDef_Name, out, n);
        }

        struct Category { uint16_t row; uint16_t order; uint16_t tabRow; uint16_t tabOrder; };
        constexpr Category kNoCategory{ 0xFFFF, kGrpOrder_Internal, 0xFFFF, kGrpOrder_Internal };

        bool GroupOrder(uint16_t row, uint16_t* order)
        {
            uintptr_t grp = 0;
            if (!DefForRow(g_grpTableGlobal, row, &grp)) return false;
            return Read16(grp + kOff_GrpDef_Order, order);
        }

        bool CategoryOfType(uint16_t typeId, Category* out)
        {
            uintptr_t def = 0;
            if (!DefForRow(g_itemTableGlobal, typeId, &def)) return false;
            uintptr_t rows = 0;
            uint32_t  count = 0;
            if (!ReadPtr(def + kOff_ItemDef_Groups + kOff_Vec_Data, &rows)) return false;
            if (!Read32(def + kOff_ItemDef_Groups + kOff_Vec_Count, &count)) return false;
            if (rows < kMinPointer || count == 0 || count > 4096) return false;

            Category c = kNoCategory;
            for (uint32_t i = 0; i < count; ++i)
            {
                uint16_t row = 0, order = 0;
                if (!Read16(rows + static_cast<uintptr_t>(i) * 2, &row)) continue;
                if (!GroupOrder(row, &order) || order == kGrpOrder_Internal) continue;
                if (order <= kGrpOrder_MaxTopTab)
                {
                    if (order < c.tabOrder) { c.tabRow = row; c.tabOrder = order; }
                }
                else if (order < c.order) { c.row = row; c.order = order; }
            }
            if (c.row == 0xFFFF) return false;
            *out = c;
            return true;
        }

        bool GroupName(uint16_t row, char* out, size_t n)
        {
            uintptr_t grp = 0;
            if (!DefForRow(g_grpTableGlobal, row, &grp)) return false;
            return LocString(grp + kOff_GrpDef_Name, out, n);
        }

        bool IconNameForRow(uint16_t row, char* out, size_t n)
        {
            if (row == kIconPath_None) return false;
            uintptr_t def = 0;
            if (!DefForRow(g_strTableGlobal, row, &def)) return false;
            if (!StringField(def + kOff_StrDef_Buffer, out, n)) return false;
            return out[0] != 0;
        }

        bool IconForType(uint16_t typeId, char* out, size_t n)
        {
            uintptr_t def = 0;
            if (!DefForRow(g_itemTableGlobal, typeId, &def)) return false;
            uintptr_t list = 0;
            uint32_t  count = 0;
            if (!ReadPtr(def + kOff_ItemDef_Icons + kOff_Vec_Data, &list)) return false;
            if (!Read32(def + kOff_ItemDef_Icons + kOff_Vec_Count, &count)) return false;
            if (list < kMinPointer || count == 0 || count > 64) return false;
            uint16_t row = 0;
            if (!Read16(list + kOff_IconData_Path, &row)) return false;
            return IconNameForRow(row, out, n);
        }

        bool IconForGroup(uint16_t groupRow, char* out, size_t n)
        {
            uintptr_t grp = 0;
            if (!DefForRow(g_grpTableGlobal, groupRow, &grp)) return false;
            uint16_t row = 0;
            if (Read16(grp + kOff_GrpDef_Icon, &row) && IconNameForRow(row, out, n))
                return true;

            char key[160];
            if (!StringField(grp + kOff_GrpDef_Key, key, sizeof(key))) return false;
            const size_t pfx = strlen(kGrpKey_SubCatPrefix);
            if (_strnicmp(key, kGrpKey_SubCatPrefix, pfx) != 0 || !key[pfx]) return false;
            snprintf(out, n, "%s%s", kIconPrefix_ItemGroup, key + pfx);
            return true;
        }

        uint8_t TierOfType(uint16_t typeId)
        {
            uintptr_t def = 0;
            if (!DefForRow(g_itemTableGlobal, typeId, &def)) return 0;
            uint16_t w = 0; 
            if (!Read16(def + kOff_ItemDef_Tier, &w)) return 0;
            return static_cast<uint8_t>(w & 0xFF);
        }

        void Prettify(const char* key, char* out, size_t n)
        {
            size_t o = 0;
            for (size_t i = 0; key[i] && o < n - 1; ++i)
            {
                const char c = key[i];
                if (c == '_') { if (o && out[o - 1] != ' ') out[o++] = ' '; continue; }
                const char prev = key[i ? i - 1 : 0];
                if (i && o < n - 1 &&
                    (islower(static_cast<unsigned char>(prev)) || isdigit(static_cast<unsigned char>(prev))) &&
                    isupper(static_cast<unsigned char>(c)))
                    out[o++] = ' ';
                if (o < n - 1) out[o++] = c;
            }
            out[o] = 0;
            if (o == 0) snprintf(out, n, "%s", key);
        }

        struct StorageStyle { const char* key; const char* name; };
        constexpr StorageStyle kStorageStyle[] = {
            { "Character",          "Inventario"   },
            { "Quest",              "Objetos de misión" },
            { "Kuku",               nullptr       }, 
            { "BirdFeed",           nullptr       }, 
            { "WareHouse",          nullptr       }, 
            { "CampWareHouse",      nullptr       },
            { "Housing_Dresser",    nullptr       }, 
            { "Bank",               nullptr       }, 
            { "Money",              "Campamento"        }, 
            { "InvisibleInventory", "Oculto"   },
        };
        constexpr int kStorageStyleCount =
            static_cast<int>(sizeof(kStorageStyle) / sizeof(kStorageStyle[0]));

        int StorageRank(const char* key)
        {
            for (int i = 0; i < kStorageStyleCount; ++i)
                if (strcmp(kStorageStyle[i].key, key) == 0) return i;
            return kStorageStyleCount;
        }

        const char* StorageStyleName(const char* key)
        {
            for (const auto& s : kStorageStyle)
                if (strcmp(s.key, key) == 0) return s.name;
            return nullptr;
        }

        bool StorageNameForType(uint16_t type, char* out, size_t n)
        {
            uintptr_t def = 0;
            if (!DefForRow(g_invTableGlobal, type, &def)) return false;
            return LocString(def + kOff_InvDef_Name, out, n);
        }

        bool StorageKeyForType(uint16_t type, char* out, size_t n)
        {
            uintptr_t def = 0;
            if (!DefForRow(g_invTableGlobal, type, &def)) return false;
            return StringField(def + kOff_InvDef_Key, out, n);
        }

        bool StorageSlotsForType(uint16_t type, uint16_t* def_, uint16_t* max_)
        {
            uintptr_t def = 0;
            if (!DefForRow(g_invTableGlobal, type, &def)) return false;
            return Read16(def + kOff_InvDef_DefSlots, def_) &&
                   Read16(def + kOff_InvDef_MaxSlots, max_);
        }

        struct Item { uintptr_t slot; uint16_t typeId; int64_t qty; char name[64];
                      char key[64]; char icon[96]; uint32_t bucketIdx; uint16_t slotIdx;
                      Category cat; uint8_t tier; };
        struct Group { Category cat; char label[48]; char tab[32]; char icon[96];
                       std::vector<Item> items; };
        struct Storage { uint16_t type; char name[96]; char key[48]; int rank;
                         uint16_t defSlots; uint16_t maxSlots; bool haveSlots;
                         bool gameNamed; std::vector<Group> groups; };
        std::vector<Storage> g_storages;
        ULONGLONG g_lastRefresh = 0;

        Storage* StorageAt(int st)
        {
            return (st >= 0 && st < static_cast<int>(g_storages.size())) ? &g_storages[st] : nullptr;
        }
        Group* GroupAt(int st, int cat)
        {
            Storage* s = StorageAt(st);
            if (!s || cat < 0 || cat >= static_cast<int>(s->groups.size())) return nullptr;
            return &s->groups[cat];
        }
        Item* ItemAt(int st, int cat, int idx)
        {
            Group* g = GroupAt(st, cat);
            if (!g || idx < 0 || idx >= static_cast<int>(g->items.size())) return nullptr;
            return &g->items[idx];
        }

        bool HolderLooksValid(uintptr_t holder)
        {
            if (holder < kMinPointer) return false;
            uintptr_t buckets = 0;
            uint32_t  bcount  = 0;
            if (!ReadPtr(holder + kOff_InvHolder_Buckets, &buckets)) return false;
            if (!Read32(holder + kOff_InvHolder_Count, &bcount)) return false;
            return buckets >= kMinPointer && bcount > 0 && bcount <= 4096;
        }

        uintptr_t HolderForContainer(uintptr_t container)
        {
            if (container < kMinPointer) return 0;
            uintptr_t sub = 0, holder = 0;
            if (!ReadPtr(container + kOff_Container_Sub, &sub) || sub < kMinPointer) return 0;
            if (!ReadPtr(sub + kOff_Sub_Holder, &holder)) return 0;
            return HolderLooksValid(holder) ? holder : 0;
        }

        uintptr_t ResolveHolderByWalk()
        {
            if (!g_coreGlobal) return 0;
            uintptr_t g = 0, mid = 0, container = 0;
            if (!ReadPtr(g_coreGlobal, &g) || g < kMinPointer) return 0;
            if (!ReadPtr(g + kOff_Global_Mid, &mid) || mid < kMinPointer) return 0;
            if (!ReadPtr(mid + kOff_Mid_Container, &container) || container < kMinPointer) return 0;
            return HolderForContainer(container);
        }

        uintptr_t ResolveClientContainer()
        {
            if (!g_coreGlobal) return 0;
            uintptr_t g = 0, mid = 0, container = 0;
            if (!ReadPtr(g_coreGlobal, &g) || g < kMinPointer) return 0;
            if (!ReadPtr(g + kOff_Global_Mid, &mid) || mid < kMinPointer) return 0;
            if (!ReadPtr(mid + kOff_Mid_Container, &container) || container < kMinPointer) return 0;
            return container;
        }

        uintptr_t CurrentHolder(); 

        uint32_t HolderBucketCount(uintptr_t holder)
        {
            if (!HolderLooksValid(holder)) return 0;
            uintptr_t buckets = 0;
            uint32_t  bcount  = 0;
            if (!ReadPtr(holder + kOff_InvHolder_Buckets, &buckets)) return 0;
            if (!Read32(holder + kOff_InvHolder_Count, &bcount)) return 0;
            if (buckets < kMinPointer || bcount == 0 || bcount > 4096) return 0;
            return bcount;
        }

        using NtQueryInformationThread_t = LONG(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
        NtQueryInformationThread_t oNtQueryInfoThread = nullptr;

        bool RawReadPtr(uintptr_t addr, uintptr_t* out) {
            return out && ds::leer(addr, *out);
        }
        bool RawRead8(uintptr_t addr, uint8_t* out) {
            return out && ds::leer(addr, *out);
        }
        bool RawWrite8(uintptr_t addr, uint8_t val) {
            return ds::escribir(addr, val);
        }

        uintptr_t RealmFlagAddr(uint8_t* outVal = nullptr)
        {
            if (outVal) *outVal = 0xFF;
            if (!oNtQueryInfoThread) return 0;
            
            uint8_t tbi[48] = {};
            if (oNtQueryInfoThread(GetCurrentThread(), 0 ,
                                   tbi, sizeof(tbi), nullptr) < 0)
                return 0;
            uintptr_t teb = 0;
            memcpy(&teb, tbi + 8, sizeof(teb));
            if (!teb) return 0;
            uintptr_t tlsArray = 0, tls = 0;
            if (!RawReadPtr(teb + kOff_Teb_TlsPointer, &tlsArray) || !tlsArray) return 0;
            if (!RawReadPtr(tlsArray, &tls) || !tls) return 0;
            const uintptr_t addr = tls + kTls_RealmFlag;
            uint8_t v = 0;
            if (!RawRead8(addr, &v)) return 0;
            if (outVal) *outVal = v;
            if (v > 1) return 0; 
            return addr;
        }

        bool IsLiveCharacter(uintptr_t c)
        {
            if (c < kMinPointer) return false;
            uintptr_t possessor = 0;
            if (!ReadPtr(c + kOff_Owner_Possessor, &possessor) || possessor < kMinPointer) return false;
            uintptr_t pawn = 0;
            if (!ReadPtr(possessor + kOff_Possessor_Pawn, &pawn)) return false;
            return pawn == c;
        }

        int SnapshotCandidates(Candidate* out)
        {
            if (!g_candLockInit) return 0;
            EnterCriticalSection(&g_candLock);
            const int n = g_candCount.load(std::memory_order_relaxed);
            for (int i = 0; i < n; ++i) out[i] = g_cand[i];
            LeaveCriticalSection(&g_candLock);
            return n;
        }

        bool EsMirror(uintptr_t cliente, uintptr_t candidato) {
            uintptr_t ca = 0, sa = 0;
            uint32_t cn = 0, sn = 0;
            if (!ReadPtr(cliente + kOff_InvHolder_Buckets, &ca) ||
                !ReadPtr(candidato + kOff_InvHolder_Buckets, &sa) ||
                !Read32(cliente + kOff_InvHolder_Count, &cn) ||
                !Read32(candidato + kOff_InvHolder_Count, &sn) || !cn || cn > 4096 || cn != sn) return false;
            size_t identidades = 0;
            for (uint32_t b = 0; b < cn; ++b) {
                uintptr_t cb = 0, sb = 0, cs = 0, ss = 0;
                uint16_t ct = 0, st = 0, nc = 0, ns = 0;
                if (!ReadPtr(ca + 8ull*b, &cb) || !ReadPtr(sa + 8ull*b, &sb) ||
                    !Read16(cb + kOff_InvBucket_Type, &ct) || !Read16(sb + kOff_InvBucket_Type, &st) || ct != st ||
                    !Read16(cb + kOff_InvBucket_Count, &nc) || !Read16(sb + kOff_InvBucket_Count, &ns) ||
                    nc != ns || nc > 8192 ||
                    !ReadPtr(cb + kOff_InvBucket_Slots, &cs) || !ReadPtr(sb + kOff_InvBucket_Slots, &ss)) return false;
                for (uint16_t i = 0; i < nc; ++i) {
                    const auto cp = cs + static_cast<uintptr_t>(i)*kInvSlot_Stride;
                    const auto sp = ss + static_cast<uintptr_t>(i)*kInvSlot_Stride;
                    uint16_t ti = 0, si = 0;
                    uint64_t ci = 0, ii = 0;
                    if (!Read16(cp + kOff_InvSlot_TypeId, &ti) || !Read16(sp + kOff_InvSlot_TypeId, &si) || ti != si) return false;
                    if (ti == kInvSlot_EmptyType) continue;
                    if (!Read64(cp + kOff_ItemVal_InstanceId, &ci) || !Read64(sp + kOff_ItemVal_InstanceId, &ii) || ci != ii) return false;
                    if (ci && ci != UINT64_MAX) ++identidades;
                }
            }
            return identidades > 0;
        }
        uintptr_t ServerHolder() {
            const uintptr_t clientC = ResolveClientContainer();
            const uintptr_t clientH = CurrentHolder();
            if (!clientC || !clientH) return 0;
            Candidate snap[kMaxCandidates] = {};
            const int n = SnapshotCandidates(snap);
            uintptr_t candidato = 0, contenedor = 0;
            for (int i = 0; i < n; ++i) {
                const uintptr_t c = snap[i].container;
                if (!c || c == clientC || !IsLiveCharacter(c)) continue;
                const uintptr_t h = HolderForContainer(c);
                if (!h || h == clientH || !EsMirror(clientH, h)) continue;
                if (candidato && candidato != h) return 0;
                candidato = h; contenedor = c;
            }
            g_serverHolder.store(candidato, std::memory_order_release);
            g_serverContainer.store(contenedor, std::memory_order_release);
            return candidato;
        }

        uintptr_t CurrentHolder() {
            const uintptr_t personaje = ResolveClientContainer();
            if (!IsLiveCharacter(personaje)) return 0;
            const uintptr_t h = HolderForContainer(personaje);
            if (h) g_holder.store(h, std::memory_order_release);
            return h;
        }

        int64_t __fastcall hkGetItemQty(void* container, uint16_t typeId, void* keyPtr)
        {
            const auto resultado = oGetItemQty(container, typeId, keyPtr);
            // Hasta publicar la instalación completa no se leen los adaptadores en preparación.
            if (!ds::inventarioActivo()) return resultado;
            if (container && oGetHolder)
            {
                const ULONGLONG now = GetTickCount64();

                if (g_holder.load(std::memory_order_relaxed) < kMinPointer ||
                    now - g_holderTick.load(std::memory_order_relaxed) > 500)
                {
                    g_holderTick.store(now, std::memory_order_relaxed);

                    void* h = nullptr;
                    __try { h = oGetHolder(container); }
                    __except (EXCEPTION_EXECUTE_HANDLER) { h = nullptr; }
                    if (reinterpret_cast<uintptr_t>(h) >= kMinPointer)
                        g_holder.store(reinterpret_cast<uintptr_t>(h), std::memory_order_release);
                }
            }
            /* El trabajo pendiente se ejecuta únicamente desde MoveUpdate. */
            return resultado;
        }

        void PruneDeadCandidates()
        {
            const ULONGLONG now = GetTickCount64();
            const int cnt = g_candCount.load(std::memory_order_relaxed);
            int keep = 0;
            for (int i = 0; i < cnt; ++i)
            {
                if (now - g_cand[i].tick > kCandGraceMs && !IsLiveCharacter(g_cand[i].container))
                    continue;
                g_cand[keep++] = g_cand[i];
            }
            for (int i = keep; i < cnt; ++i) g_cand[i] = Candidate{};
            g_candCount.store(keep, std::memory_order_release);
        }

        void NoteContainer(void* container)
        {
            const uintptr_t c = reinterpret_cast<uintptr_t>(container);
            if (c < kMinPointer || !oGetHolder || !g_candLockInit) return;

            void* h = nullptr;
            __try { h = oGetHolder(container); }
            __except (EXCEPTION_EXECUTE_HANDLER) { h = nullptr; }
            if (reinterpret_cast<uintptr_t>(h) < kMinPointer) return;

            const ULONGLONG now = GetTickCount64();
            EnterCriticalSection(&g_candLock);
            int cnt = g_candCount.load(std::memory_order_relaxed);
            int at  = -1;
            for (int i = 0; i < cnt; ++i)
                if (g_cand[i].container == c) { at = i; break; }
            if (at < 0 && cnt >= kMaxCandidates)
            {
                PruneDeadCandidates(); 
                cnt = g_candCount.load(std::memory_order_relaxed);
            }
            if (at < 0 && cnt < kMaxCandidates) at = cnt;
            if (at >= 0)
            {
                g_cand[at].container = c;
                g_cand[at].holder    = reinterpret_cast<uintptr_t>(h);
                g_cand[at].tick      = now;
                if (at >= cnt) g_candCount.store(at + 1, std::memory_order_release); 
            }
            LeaveCriticalSection(&g_candLock);
        }

        void* __fastcall hkCommit(void* holder, void* err, void* container, void* items,
                                  void* out, uint8_t a6, uint8_t a7)
        {
            NoteContainer(container);
            return oCommit(holder, err, container, items, out, a6, a7);
        }

        void* __fastcall hkHolderInsert(void* bucket, void* err, void* container, void* itemArr,
                                        uint16_t a5, void* a6, uint8_t a7, uint8_t a8, uint8_t a9)
        {
            NoteContainer(container);
            return oHolderInsert(bucket, err, container, itemArr, a5, a6, a7, a8, a9);
        }

        bool OverrideExpandForType(uint16_t type, int value, uint16_t* out,
                                   uint16_t* outDef = nullptr)
        {
            uint16_t defSlots = 0, maxSlots = 0;
            if (!StorageSlotsForType(type, &defSlots, &maxSlots)) return false;
            if (value < 1) value = 1;
            if (value > 0xFFFF) value = 0xFFFF;
            *out = (value > defSlots) ? static_cast<uint16_t>(value - defSlots) : 0;
            if (outDef) *outDef = defSlots;
            return true;
        }

        uintptr_t BucketByType(uintptr_t holder, uint16_t type)
        {
            uintptr_t buckets = 0;
            uint32_t  bcount  = 0;
            if (!ReadPtr(holder + kOff_InvHolder_Buckets, &buckets) || buckets < kMinPointer) return 0;
            if (!Read32(holder + kOff_InvHolder_Count, &bcount) || bcount == 0 || bcount > 4096) return 0;
            for (uint32_t b = 0; b < bcount; ++b)
            {
                uintptr_t bucket = 0;
                if (!ReadPtr(buckets + static_cast<uintptr_t>(b) * 8, &bucket) || bucket < kMinPointer) continue;
                uint16_t t = 0;
                if (Read16(bucket + kOff_InvBucket_Type, &t) && t == type) return bucket;
            }
            return 0;
        }

        void* __fastcall hkSetExpandSlots(void* holder, int* outErr, void* a3,
                                          uint16_t type, uint16_t count)
        {
            const State& st = State::Get();
            if (st.invSlotSize)
            {
                uint16_t expand = 0;
                if (OverrideExpandForType(type, st.invSlotSizeVal, &expand))
                {
                    const uintptr_t bucket = BucketByType(reinterpret_cast<uintptr_t>(holder), type);
                    if (bucket)
                        UpsertOrigExpand(bucket, type, count);
                    count = expand;
                }
            }
            return oSetExpandSlots(holder, outErr, a3, type, count);
        }

        bool RepairUsedSlots(uintptr_t holder)
        {
            if (!HolderLooksValid(holder)) return false;
            uintptr_t buckets = 0;
            uint32_t bcount = 0;
            if (!ReadPtr(holder + kOff_InvHolder_Buckets, &buckets) ||
                !Read32(holder + kOff_InvHolder_Count, &bcount) || bcount == 0 || bcount > 4096) return false;
            for (uint32_t b = 0; b < bcount; ++b) {
                uintptr_t bucket = 0;
                if (!ReadPtr(buckets + static_cast<uintptr_t>(b) * 8, &bucket)) return false;
                if (bucket < kMinPointer) continue;
                uint16_t type = 0, used = 0;
                if (!Read16(bucket + kOff_InvBucket_Type, &type)) return false;
                if (type == kInvSlot_EmptyType) continue;
                if (!Read16(bucket + kOff_InvBucket_UsedSlots, &used)) return false;
                uintptr_t slots = 0;
                uint16_t scount = 0;
                if (!ReadPtr(bucket + kOff_InvBucket_Slots, &slots) ||
                    !Read16(bucket + kOff_InvBucket_Count, &scount) || scount > 4096) return false;
                if (!scount) continue;
                if (slots < kMinPointer) return false;
                uint16_t occupied = 0;
                for (uint16_t i = 0; i < scount; ++i) {
                    const uintptr_t slot = slots + static_cast<uintptr_t>(i) * kInvSlot_Stride;
                    uint16_t tid = 0; int64_t qty = 0;
                    if (!Read16(slot + kOff_InvSlot_TypeId, &tid)) return false;
                    if (tid == kInvSlot_EmptyType) continue;
                    if (!Read64(slot + kOff_InvSlot_Quantity, &qty)) return false;
                    if (qty > 0) ++occupied;
                }
                // Conserva el alcance del procedimiento publicado: reparar bajas de ocupación.
                if (occupied < used && !Write16(bucket + kOff_InvBucket_UsedSlots, occupied)) return false;
            }
            return true;
        }

        uintptr_t FindItemPrologueAbove(uintptr_t lea)
        {
            for (size_t back = 0x20; back <= 0x80; ++back)
            {
                const uintptr_t cand = lea - back;
                bool hit = true;
                __try
                {
                    for (size_t i = 0; i < sizeof(kItemPrologue); ++i)
                        if (*reinterpret_cast<const uint8_t*>(cand + i) != kItemPrologue[i]) { hit = false; break; }
                }
                __except (EXCEPTION_EXECUTE_HANDLER) { hit = false; }
                if (hit) return cand;
            }
            return 0;
        }

        struct TableHunt { const char* name; bool indirect; uintptr_t fn; };

        bool IsTableRef(uintptr_t match, void* ctx)
        {
            auto* h = static_cast<TableHunt*>(ctx);
            uintptr_t target = mem::ResolveRipAt(match, 7);

            if (h->indirect && (!ReadPtr(target, &target) || target < kMinPointer)) return false;
            char buf[32];
            if (!ReadCString(target, buf, sizeof(buf))) return false;
            if (strcmp(buf, h->name) != 0) return false;

            const uintptr_t fn = FindItemPrologueAbove(match);
            if (!fn) return false; 
            h->fn = fn;
            return true;
        }

        uintptr_t FindTableGlobal(const char* name, bool indirect = false)
        {
            TableHunt hunt{ name, indirect, 0 };
            const auto unico = mem::FindPatternIf(indirect ? kSig_MovR8Rip : kSig_LeaR8Rip, &IsTableRef, &hunt);
            return unico && hunt.fn ? mem::ResolveRipAt(hunt.fn + kOff_ItemResolver_MovGlobal, 7) : 0;
        }
    }

    bool Inventory::Install()
    {
        if (!mem::InstallHook("inventario: consulta de cantidad", kSig_InvGetItemQty, "inventario desactivado",
                              &hkGetItemQty, &oGetItemQty, &g_qtyTarget, 4))
            return false;

        const uintptr_t holderAddr = mem::FindPattern(kSig_InvGetHolder);
        if (!holderAddr)
        {
            informarMotor();
            return false;
        }
        oGetHolder = reinterpret_cast<GetHolder_t>(holderAddr);

        const uintptr_t ctorAddr   = mem::FindPattern(kSig_TrItemValueCtor);
        const uintptr_t commitAddr = mem::FindPattern(kSig_InvCommitPlacement);
        const uintptr_t freeAddr   = mem::FindPattern(kSig_InvFreePlacements);
        const uintptr_t dtorAddr   = mem::FindPattern(kSig_TrItemValueDtor);
        if (ctorAddr)   oItemValueCtor   = reinterpret_cast<ItemValueCtor_t>(ctorAddr);
        if (commitAddr) oCommitPlacement = reinterpret_cast<CommitPlacement_t>(commitAddr);
        if (freeAddr)   oFreePlacements  = reinterpret_cast<FreePlacements_t>(freeAddr);
        if (dtorAddr)   oItemValueDtor   = reinterpret_cast<ItemValueDtor_t>(dtorAddr);

        if (const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll"))
            oNtQueryInfoThread = reinterpret_cast<NtQueryInformationThread_t>(
                GetProcAddress(ntdll, "NtQueryInformationThread"));
        if (!ctorAddr || !commitAddr || !freeAddr || !dtorAddr || !oNtQueryInfoThread)
            informarMotor();

        if (!g_candLockInit)
        {
            InitializeCriticalSection(&g_candLock);
            g_candLockInit = true;
        }
        if (!g_capLockInit)
        {
            InitializeCriticalSection(&g_capLock);
            g_capLockInit = true;
        }

        if (!mem::InstallHook("inventario: expansión de ranuras", kSig_InvSetExpandSlots,
                              "no se aplicará expansión",
                              &hkSetExpandSlots, &oSetExpandSlots, &g_expandTarget, 4))
        {
            const uintptr_t expandAddr = mem::FindPattern(kSig_InvSetExpandSlots);
            if (expandAddr)
            {
                oSetExpandSlots = reinterpret_cast<SetExpandSlots_t>(expandAddr);
                informarMotor();
            }
        }

        mem::InstallHook("inventario: confirmación de transacción", kSig_InvCommit,
                         "la edición no tiene mirror de autoridad",
                         &hkCommit, &oCommit, &g_commitTarget, 4);

        mem::InstallHook("inventario: planeación de inserción", kSig_InvHolderInsert,
                         "la autoridad depende de la captura de transacciones",
                         &hkHolderInsert, &oHolderInsert, &g_insTarget, 4);

        const uintptr_t globAnchor = mem::FindPattern(kSig_InvCoreGlobal);
        if (globAnchor)
            g_coreGlobal = mem::ResolveRipAt(globAnchor + kOff_InvCoreGlobal_Mov, 7);
        if (!g_coreGlobal)
            informarMotor();

        g_itemTableGlobal = FindTableGlobal(kStr_ItemInfoTable);
        if (!g_itemTableGlobal)
            informarMotor();

        g_grpTableGlobal = FindTableGlobal(kStr_ItemGroupInfoTable);
        if (!g_grpTableGlobal)
            informarMotor();

        g_strTableGlobal = FindTableGlobal(kStr_StringInfoTable);
        if (!g_strTableGlobal)
            informarMotor();

        g_invTableGlobal = FindTableGlobal(kStr_InventoryInfoTable, true);
        if (!g_invTableGlobal)
            informarMotor();

        const uintptr_t locGet = mem::FindPattern(kSig_LocStringGet);
        if (locGet)
            g_locMgrGlobal = mem::ResolveRipAt(locGet + kOff_LocGet_MovGlobal, 7);
        if (!g_locMgrGlobal)
            informarMotor();

        return true;
    }

    void Inventory::Remove()
    {

        if (g_stackApplied) { SetAllMaxStackSizes(false, 0); g_stackApplied = false; }
        if (g_slotApplied)  { SetAllSlotSizes(false, 0);     g_slotApplied  = false; }

        mem::RemoveHook(&g_qtyTarget);
        mem::RemoveHook(&g_insTarget);
        mem::RemoveHook(&g_commitTarget);
        mem::RemoveHook(&g_expandTarget); 
                                          
        g_holder.store(0);
        g_serverHolder.store(0);
        g_serverContainer.store(0);
        g_candCount.store(0);
        g_storages.clear();
    }

    bool Inventory::Ready()
    {
        return CurrentHolder() != 0;
    }

    static void RefreshImpl(bool force)
    {
        const ULONGLONG now = GetTickCount64();
        if (!force && now - g_lastRefresh < 120) return; 
        g_lastRefresh = now;

        g_storages.clear();

        const uintptr_t holder = CurrentHolder();
        if (!holder) return;

        uintptr_t buckets = 0;
        uint32_t  bcount  = 0;
        if (!ReadPtr(holder + kOff_InvHolder_Buckets, &buckets)) return;
        if (!Read32(holder + kOff_InvHolder_Count, &bcount)) return;
        if (buckets < kMinPointer || bcount == 0 || bcount > 4096) return;

        for (uint32_t b = 0; b < bcount; ++b)
        {
            uintptr_t bucket = 0;
            if (!ReadPtr(buckets + static_cast<uintptr_t>(b) * 8, &bucket) || bucket < kMinPointer) continue;

            uint16_t stype = 0;
            if (!Read16(bucket + kOff_InvBucket_Type, &stype)) continue;

            uintptr_t slots = 0;
            uint16_t  scount = 0;
            if (!ReadPtr(bucket + kOff_InvBucket_Slots, &slots) || slots < kMinPointer) continue;
            if (!Read16(bucket + kOff_InvBucket_Count, &scount) || scount == 0 || scount > 8192) continue;

            Storage store{};
            store.type = stype;

            if (!StorageKeyForType(stype, store.key, sizeof(store.key)))
                store.key[0] = 0;
            store.rank      = store.key[0] ? StorageRank(store.key) : kStorageStyleCount;
            store.gameNamed = StorageNameForType(stype, store.name, sizeof(store.name));
            if (!store.gameNamed)
            {
                if (store.key[0])
                    Prettify(store.key, store.name, sizeof(store.name));
                else
                    snprintf(store.name, sizeof(store.name), "Almacén #%u", stype);
            }
            store.haveSlots = StorageSlotsForType(stype, &store.defSlots, &store.maxSlots);

            for (uint16_t i = 0; i < scount; ++i)
            {
                const uintptr_t slot = slots + static_cast<uintptr_t>(i) * kInvSlot_Stride;
                uint16_t tid = 0;
                int64_t  qty = 0;
                if (!Read16(slot + kOff_InvSlot_TypeId, &tid) || tid == kInvSlot_EmptyType) continue;
                if (!Read64(slot + kOff_InvSlot_Quantity, &qty) || qty <= 0) continue;

                Item it{};
                it.slot      = slot;
                it.typeId    = tid;
                it.qty       = qty;
                it.bucketIdx = b;
                it.slotIdx   = i;
                it.tier      = TierOfType(tid);

                if (!DisplayNameForType(tid, it.name, sizeof(it.name)))
                {
                    if (KeyForType(tid, it.key, sizeof(it.key)))
                        Prettify(it.key, it.name, sizeof(it.name));
                    else
                        snprintf(it.name, sizeof(it.name), "Objeto #%u", tid);
                }
                if (!it.key[0] && !KeyForType(tid, it.key, sizeof(it.key)))
                    it.key[0] = 0;
                if (!IconForType(tid, it.icon, sizeof(it.icon)))
                    it.icon[0] = 0; 

                if (!CategoryOfType(tid, &it.cat))
                    it.cat = kNoCategory; 

                Group* g = nullptr;
                for (auto& cand : store.groups)
                    if (cand.cat.row == it.cat.row) { g = &cand; break; }
                if (!g)
                {
                    Group ng{};
                    ng.cat = it.cat;
                    if (!GroupName(it.cat.row, ng.label, sizeof(ng.label)))
                        snprintf(ng.label, sizeof(ng.label), "Sin categoría");
                    if (it.cat.tabRow == 0xFFFF ||
                        !GroupName(it.cat.tabRow, ng.tab, sizeof(ng.tab)))
                        ng.tab[0] = 0; 
                    if (!IconForGroup(it.cat.row, ng.icon, sizeof(ng.icon)))
                        snprintf(ng.icon, sizeof(ng.icon), "%s", kIcon_Uncategorised);
                    store.groups.push_back(std::move(ng));
                    g = &store.groups.back();
                }
                g->items.push_back(it);
            }

            if (store.groups.empty()) continue; 

            for (auto& g : store.groups)
                std::sort(g.items.begin(), g.items.end(), [](const Item& a, const Item& b) {
                    return _stricmp(a.name, b.name) < 0;
                });

            std::sort(store.groups.begin(), store.groups.end(), [](const Group& a, const Group& b) {
                if (a.cat.tabOrder != b.cat.tabOrder) return a.cat.tabOrder < b.cat.tabOrder;
                return a.cat.order < b.cat.order;
            });

            g_storages.push_back(std::move(store));
        }

        std::sort(g_storages.begin(), g_storages.end(), [](const Storage& a, const Storage& b) {
            if (a.rank != b.rank) return a.rank < b.rank;
            return a.type < b.type;
        });

        const size_t n = g_storages.size();
        std::vector<bool> ambiguous(n, false);
        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j < n; ++j)
                if (i != j && strcmp(g_storages[i].name, g_storages[j].name) == 0)
                {
                    ambiguous[i] = true;
                    break;
                }
        for (size_t i = 0; i < n; ++i)
        {
            Storage& s = g_storages[i];
            if (!ambiguous[i] && s.gameNamed) continue; 
            if (!s.key[0]) continue;                    
            if (const char* ours = StorageStyleName(s.key))
            {
                snprintf(s.name, sizeof(s.name), "%s", ours);
                continue;
            }
            if (!ambiguous[i]) continue; 

            char q[sizeof(s.name)];
            snprintf(q, sizeof(q), "%s (%s)", s.name, s.key);
            snprintf(s.name, sizeof(s.name), "%s", q);
        }
    }

    void Inventory::Refresh()      { RefreshImpl(false); }
    void Inventory::ForceRefresh() { RefreshImpl(true); }

    int Inventory::StorageCount() { return static_cast<int>(g_storages.size()); }

    const char* Inventory::StorageName(int st)
    {
        const Storage* s = StorageAt(st);
        return s ? s->name : "";
    }

    const char* Inventory::StorageKey(int st)
    {
        const Storage* s = StorageAt(st);
        return s ? s->key : "";
    }

    bool Inventory::StorageSlots(int st, int* defaultSlots, int* maxSlots)
    {
        const Storage* s = StorageAt(st);
        if (!s || !s->haveSlots) return false;
        if (defaultSlots) *defaultSlots = s->defSlots;
        if (maxSlots)     *maxSlots     = s->maxSlots;
        return true;
    }

    int Inventory::StorageItemCount(int st)
    {
        const Storage* s = StorageAt(st);
        if (!s) return 0;
        int n = 0;
        for (const auto& g : s->groups) n += static_cast<int>(g.items.size());
        return n;
    }

    int Inventory::CategoryCount(int st)
    {
        const Storage* s = StorageAt(st);
        return s ? static_cast<int>(s->groups.size()) : 0;
    }

    const char* Inventory::CategoryName(int st, int cat)
    {
        const Group* g = GroupAt(st, cat);
        return g ? g->label : "";
    }

    int Inventory::ItemCount(int st, int cat)
    {
        const Group* g = GroupAt(st, cat);
        return g ? static_cast<int>(g->items.size()) : 0;
    }

    bool Inventory::GetItem(int st, int cat, int idx, const char** name, int64_t* qty,
                            const char** icon)
    {
        const Item* it = ItemAt(st, cat, idx);
        if (!it) return false;
        if (name) *name = it->name;
        if (qty)  *qty  = it->qty;
        if (icon) *icon = it->icon;
        return true;
    }

    bool Inventory::GetItemInfo(int st, int cat, int idx, ItemInfo* out)
    {
        if (!out) return false;
        const Item* it = ItemAt(st, cat, idx);
        if (!it) return false;
        out->name   = it->name;
        out->key    = it->key;
        out->icon   = it->icon;
        out->qty    = it->qty;
        out->typeId = it->typeId;
        out->tier   = it->tier;
        return true;
    }

    const char* Inventory::CategoryTab(int st, int cat)
    {
        const Group* g = GroupAt(st, cat);
        return g ? g->tab : "";
    }

    const char* Inventory::CategoryIcon(int st, int cat)
    {
        const Group* g = GroupAt(st, cat);
        return g ? g->icon : "";
    }

    bool Inventory::SetAllMaxStackSizes(bool enable, int64_t value)
    {
        if (!g_itemTableGlobal) return false;
        uintptr_t table = 0;
        if (!ReadPtr(g_itemTableGlobal, &table)) return false;
        uint32_t count = 0;
        if (!Read32(table + kOff_ItemTable_Count, &count) || count == 0 || count > 65536) return false;

        if (g_stackCaptured.size() != count)
        {
            g_origMaxStack.assign(count, 0);
            g_origApplyCap.assign(count, 0);
            g_stackCaptured.assign(count, false);
        }
        if (value < 1) value = 1;

        bool any = false;
        for (uint32_t row = 0; row < count; ++row)
        {
            uintptr_t def = 0;
            if (!DefForRow(g_itemTableGlobal, static_cast<uint16_t>(row), &def)) continue;

            if (enable)
            {
                if (!g_stackCaptured[row])
                {
                    int64_t origVal = 0;
                    uint8_t origCap = 0;
                    Read64(def + kOff_ItemDef_MaxStackCount, &origVal);
                    Read8(def + kOff_ItemDef_ApplyMaxStackCap, &origCap);
                    g_origMaxStack[row]  = origVal;
                    g_origApplyCap[row]  = origCap;
                    g_stackCaptured[row] = true;
                }
                if (Write64(def + kOff_ItemDef_MaxStackCount, static_cast<uint64_t>(value)))
                    any = true;
                Write8(def + kOff_ItemDef_ApplyMaxStackCap, 1);
            }
            else if (g_stackCaptured[row])
            {
                Write64(def + kOff_ItemDef_MaxStackCount, static_cast<uint64_t>(g_origMaxStack[row]));
                Write8(def + kOff_ItemDef_ApplyMaxStackCap, g_origApplyCap[row]);
                any = true;
            }
        }
        return any;
    }

    namespace
    {

        bool ApplySlotCapToHolder(uintptr_t holder, bool enable, uint16_t value)
        {
            if (!oSetExpandSlots) return false;
            if (!HolderLooksValid(holder)) return false;
            uintptr_t buckets = 0;
            uint32_t  bcount  = 0;
            if (!ReadPtr(holder + kOff_InvHolder_Buckets, &buckets)) return false;
            if (!Read32(holder + kOff_InvHolder_Count, &bcount) || bcount == 0 || bcount > 4096) return false;

            bool any = false;
            for (uint32_t b = 0; b < bcount; ++b)
            {
                uintptr_t bucket = 0;
                if (!ReadPtr(buckets + static_cast<uintptr_t>(b) * 8, &bucket) || bucket < kMinPointer) continue;

                uint16_t type = 0;
                if (!Read16(bucket + kOff_InvBucket_Type, &type) || type == kInvSlot_EmptyType) continue;

                uint16_t expand = 0;
                if (enable)
                {

                    uint16_t defSlots = 0;
                    if (!OverrideExpandForType(type, value, &expand, &defSlots)) continue;

                    uint16_t cap = 0;
                    if (Read16(bucket + kOff_InvBucket_MaxSlots, &cap) &&
                        cap == static_cast<uint16_t>(defSlots + expand))
                    {
                        any = true;
                        continue;
                    }

                    CaptureOrigExpandOnce(bucket, type);
                }
                else if (!FindOrigBucketCap(bucket, type, &expand))
                {
                    continue; 
                }

                int err = 0;
                oSetExpandSlots(reinterpret_cast<void*>(holder), &err, nullptr, type, expand);
                if (err == 0) any = true;
            }
            return any;
        }
    }

    bool Inventory::SetAllSlotSizes(bool enable, int value)
    {
        if (value < 1) value = 1;
        if (value > 0xFFFF) value = 0xFFFF;
        const uint16_t v = static_cast<uint16_t>(value);

        bool any = false;
        if (ApplySlotCapToHolder(CurrentHolder(), enable, v)) any = true;
        if (ApplySlotCapToHolder(ServerHolder(), enable, v))  any = true;

        if (!enable && any && g_capLockInit)
        {
            EnterCriticalSection(&g_capLock);
            g_origBucketCap.clear();
            LeaveCriticalSection(&g_capLock);
        }
        return any;
    }

    namespace { void RunPendingAdd(); } 

    void Inventory::Tick()
    {
        const State& st = State::Get();

        RunPendingAdd();

        {
            static ULONGLONG s_lastRepair = 0;
            const ULONGLONG now = GetTickCount64();
            if (now - s_lastRepair >= 1000)
            {
                s_lastRepair = now;
                RepairUsedSlots(CurrentHolder());
                RepairUsedSlots(ServerHolder());
            }
        }

        if (st.invStackSize)
        {
            if (!g_stackApplied || g_stackAppliedVal != st.invStackSizeVal)
            {
                if (SetAllMaxStackSizes(true, st.invStackSizeVal))
                {
                    g_stackApplied    = true;
                    g_stackAppliedVal = st.invStackSizeVal;
                }
            }
        }
        else if (g_stackApplied)
        {
            if (SetAllMaxStackSizes(false, 0))
                g_stackApplied = false;
        }

        if (st.invSlotSize)
        {
            if (SetAllSlotSizes(true, st.invSlotSizeVal))
            {
                g_slotApplied    = true;
                g_slotAppliedVal = st.invSlotSizeVal;
            }
        }
        else if (g_slotApplied)
        {
            if (SetAllSlotSizes(false, 0))
                g_slotApplied = false;
        }
    }

    namespace
    {

        bool WriteServerMirror(uint32_t bucketIdx, uint16_t slotIdx,
                               uint16_t typeId, int64_t oldQty, int64_t value)
        {
            const uintptr_t sh = ServerHolder();
            if (!HolderLooksValid(sh)) return false;

            uintptr_t buckets = 0;
            uint32_t  bcount  = 0;
            if (!ReadPtr(sh + kOff_InvHolder_Buckets, &buckets)) return false;
            if (!Read32(sh + kOff_InvHolder_Count, &bcount)) return false;
            if (buckets < kMinPointer || bcount == 0 || bcount > 4096) return false;

            if (bucketIdx < bcount)
            {
                uintptr_t bucket = 0;
                if (ReadPtr(buckets + static_cast<uintptr_t>(bucketIdx) * 8, &bucket) && bucket >= kMinPointer)
                {
                    uintptr_t slots = 0;
                    uint16_t  scount = 0;
                    if (ReadPtr(bucket + kOff_InvBucket_Slots, &slots) && slots >= kMinPointer &&
                        Read16(bucket + kOff_InvBucket_Count, &scount) && slotIdx < scount)
                    {
                        const uintptr_t slot = slots + static_cast<uintptr_t>(slotIdx) * kInvSlot_Stride;
                        uint16_t tid = 0;
                        if (Read16(slot + kOff_InvSlot_TypeId, &tid) && tid == typeId)
                            return Write64(slot + kOff_InvSlot_Quantity, value);
                    }
                }
            }

            uintptr_t hitSlot = 0;
            int hits = 0;
            for (uint32_t b = 0; b < bcount && hits < 2; ++b)
            {
                uintptr_t bucket = 0;
                if (!ReadPtr(buckets + static_cast<uintptr_t>(b) * 8, &bucket) || bucket < kMinPointer) continue;
                uintptr_t slots = 0;
                uint16_t  scount = 0;
                if (!ReadPtr(bucket + kOff_InvBucket_Slots, &slots) || slots < kMinPointer) continue;
                if (!Read16(bucket + kOff_InvBucket_Count, &scount) || scount == 0 || scount > 8192) continue;
                for (uint16_t i = 0; i < scount; ++i)
                {
                    const uintptr_t slot = slots + static_cast<uintptr_t>(i) * kInvSlot_Stride;
                    uint16_t tid = 0;
                    int64_t  q   = 0;
                    if (!Read16(slot + kOff_InvSlot_TypeId, &tid) || tid != typeId) continue;
                    if (!Read64(slot + kOff_InvSlot_Quantity, &q) || q != oldQty) continue;
                    hitSlot = slot;
                    if (++hits >= 2) break; 
                }
            }
            if (hits == 1 && hitSlot)
                return Write64(hitSlot + kOff_InvSlot_Quantity, value);
            return false;
        }

        uintptr_t SlotByPos(uintptr_t holder, uint32_t bucketIdx, uint16_t slotIdx,
                            uint16_t wantType)
        {
            if (!HolderLooksValid(holder)) return 0;
            uintptr_t buckets = 0;
            uint32_t  bcount  = 0;
            if (!ReadPtr(holder + kOff_InvHolder_Buckets, &buckets)) return 0;
            if (!Read32(holder + kOff_InvHolder_Count, &bcount)) return 0;
            if (buckets < kMinPointer || bucketIdx >= bcount) return 0;
            uintptr_t bucket = 0;
            if (!ReadPtr(buckets + static_cast<uintptr_t>(bucketIdx) * 8, &bucket) ||
                bucket < kMinPointer) return 0;
            uintptr_t slots  = 0;
            uint16_t  scount = 0;
            if (!ReadPtr(bucket + kOff_InvBucket_Slots, &slots) || slots < kMinPointer) return 0;
            if (!Read16(bucket + kOff_InvBucket_Count, &scount) || slotIdx >= scount) return 0;
            const uintptr_t slot = slots + static_cast<uintptr_t>(slotIdx) * kInvSlot_Stride;
            uint16_t tid = 0;
            if (!Read16(slot + kOff_InvSlot_TypeId, &tid) || tid != wantType) return 0;
            return slot;
        }
    }

    bool Inventory::SetQuantity(int st, int cat, int idx, int64_t value)
    {
        Item* ip = ItemAt(st, cat, idx);
        if (!ip) return false;
        if (value < 0) value = 0;
        Item& it = *ip;

        if (!Write64(it.slot + kOff_InvSlot_Quantity, value)) return false;

        WriteServerMirror(it.bucketIdx, it.slotIdx, it.typeId, it.qty, value);

        it.qty = value; 
        return true;
    }

    bool Inventory::EditsPersist()
    {
        return HolderLooksValid(ServerHolder());
    }

    bool Inventory::NameForTypeId(uint16_t typeId, char* out, size_t n)
    {
        if (DisplayNameForType(typeId, out, n)) return true;
        return KeyForType(typeId, out, n);
    }

    bool Inventory::IconForTypeId(uint16_t typeId, char* out, size_t n)
    {
        if (n) out[0] = 0;
        return IconForType(typeId, out, n);
    }

    uintptr_t Inventory::ClientCharacterAddr()
    {
        const uintptr_t c = ResolveClientContainer();
        return IsLiveCharacter(c) ? c : 0;
    }

    uintptr_t Inventory::ServerCharacterAddr()
    {
        ServerHolder(); 
        const uintptr_t c = g_serverContainer.load(std::memory_order_acquire);
        return IsLiveCharacter(c) ? c : 0;
    }

    uintptr_t Inventory::RealmFlagAddress(uint8_t* outVal) { return RealmFlagAddr(outVal); }

    namespace
    {

        uintptr_t BucketForItem(uintptr_t holder, uintptr_t def)
        {
            uint16_t want = 0;
            if (!Read16(def + kOff_ItemDef_BucketType, &want)) return 0;
            uintptr_t buckets = 0;
            uint32_t  n = 0;
            if (!ReadPtr(holder + kOff_InvHolder_Buckets, &buckets)) return 0;
            if (!Read32(holder + kOff_InvHolder_Count, &n) || !n || n > 4096) return 0;
            for (uint32_t i = 0; i < n; ++i)
            {
                uintptr_t b = 0;
                if (!ReadPtr(buckets + static_cast<uintptr_t>(i) * 8, &b) || b < kMinPointer) continue;
                uint16_t t = 0;
                if (Read16(b + kOff_InvBucket_Type, &t) && t == want) return b;
            }
            return 0; 
        }

        uintptr_t IdAllocator(uintptr_t container)
        {
            uintptr_t tagObj = 0;
            if (!ReadPtr(container + kOff_Owner_TypeDesc, &tagObj) || tagObj < kMinPointer) return 0;
            uint8_t tag = 0;
            if (!Read8(tagObj + 1, &tag)) return 0;
            uintptr_t owner = container;
            if ((tag & 0xF7) != 0 &&
                (!ReadPtr(container + kOff_Owner_Possessor, &owner) || owner < kMinPointer))
                return 0;
            uintptr_t sub = 0, alloc = 0;
            if (!ReadPtr(owner + kOff_Container_Sub, &sub) || sub < kMinPointer) return 0;
            if (!ReadPtr(sub + kOff_Sub_IdAllocator, &alloc) || alloc < kMinPointer) return 0;
            return alloc;
        }

        int AddInRealm(uintptr_t holder, uintptr_t container, uintptr_t bucket,
                       uint16_t typeId, int64_t qty, int64_t id, const char* realm)
        {
            alignas(16) uint8_t itemVal[kItemVal_Size] = {}; 

            uintptr_t arr[2] = {};  
            uintptr_t out[3] = {};  
            int err = 0;
            volatile int committed = 0;
            volatile int firstErr2 = 0;      
            volatile uint32_t nPlaced = 0;   
            volatile bool built = false, planned = false, excepted = false;

            __try
            {
                oItemValueCtor(itemVal, &typeId, qty);
                built = true;
                *reinterpret_cast<uint16_t*>(itemVal + kOff_ItemVal_Subtype)    = 0;
                *reinterpret_cast<int64_t*>(itemVal + kOff_ItemVal_InstanceId)  = id;

                arr[0] = reinterpret_cast<uintptr_t>(itemVal);
                reinterpret_cast<uint32_t*>(arr)[2] = 1; 
                reinterpret_cast<uint32_t*>(arr)[3] = 1; 

                oHolderInsert(reinterpret_cast<void*>(bucket), &err,
                              reinterpret_cast<void*>(container), arr, 0, out, 1, 1, 0);
                planned = true;

                if (err == 0)
                {
                    const uintptr_t p0 = out[0];
                    const uint32_t  n  = reinterpret_cast<uint32_t*>(out)[2];
                    nPlaced = n;
                    for (uint32_t i = 0; i < n && n <= 1024 && p0 >= kMinPointer; ++i)
                    {
                        const uintptr_t p = p0 + static_cast<uintptr_t>(i) * kPlacement_Stride;
                        const uint16_t slotIdx =
                            *reinterpret_cast<uint16_t*>(p + kOff_Placement_SlotIdx);
                        int err2 = 0;
                        oCommitPlacement(reinterpret_cast<void*>(holder), &err2, nullptr,
                                         reinterpret_cast<void*>(p), slotIdx);
                        if (err2 == 0) committed = committed + 1;
                        else if (!firstErr2) firstErr2 = err2;
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { excepted = true; }

            if (committed == 0)
            {
                if (excepted)
                    informarMotor();
                else if (err != 0)

                    informarMotor();
                else if (nPlaced == 0)
                    informarMotor();
                else
                    informarMotor();
            }

            if (planned) { __try { oFreePlacements(out); }     __except (EXCEPTION_EXECUTE_HANDLER) { excepted = true; } }
            if (built)   { __try { oItemValueDtor(itemVal); }  __except (EXCEPTION_EXECUTE_HANDLER) { excepted = true; } }
            if (excepted || (committed > 0 && static_cast<uint32_t>(committed) != nPlaced)) {
                ds::falloParcialInventario();
                return 0;
            }
            return committed;
        }

        bool AddIntoHolder(uintptr_t holder, bool serverRealm,
                           uint16_t typeId, int64_t qty, int64_t id, uintptr_t def)
        {
            const char* realm = serverRealm ? "autoridad" : "visual";
            uintptr_t container = 0;
            if (!ReadPtr(holder + kOff_InvHolder_Container, &container) ||
                container < kMinPointer)
            {
                informarMotor();
                return false;
            }
            const uintptr_t bucket = BucketForItem(holder, def);
            if (!bucket)
            {
                uint16_t want = 0;
                Read16(def + kOff_ItemDef_BucketType, &want);
                informarMotor();
                return false;
            }

            uint8_t flagVal = 0xFF; 
            const uintptr_t flagAddr = RealmFlagAddr(&flagVal);
            if (!flagAddr)
            {
                informarMotor();
                return false;
            }
            uint8_t prev = 0;
            if (!RawRead8(flagAddr, &prev)) return false;
            if (!RawWrite8(flagAddr, serverRealm ? 1 : 0))
            {
                informarMotor();
                return false;
            }

            int committed = 0;
            // También se restaura TLS si una asignación de C++ falla entre llamadas nativas.
            try { committed = AddInRealm(holder, container, bucket, typeId, qty, id, realm); }
            catch (...) { ds::falloParcialInventario(); }

            if (!RawWrite8(flagAddr, prev)) {
                ds::falloParcialInventario();
                return false;
            }
            return committed > 0;
        }

        struct AddRequest { uint16_t typeId; int64_t qty; };
        AddRequest             g_addReq{};
        std::atomic<bool>      g_addPending{false};
        std::atomic<int>       g_addState{0}; 

        bool CommitAdd(uint16_t typeId, int64_t qty)
        {
            const bool ready = oItemValueCtor && oHolderInsert && oCommitPlacement &&
                               oFreePlacements && oItemValueDtor && oNtQueryInfoThread;
            uintptr_t def = 0;
            const uintptr_t clientH = CurrentHolder();
            const uintptr_t serverH = ServerHolder();
            if (!ready || !DefForRow(g_itemTableGlobal, typeId, &def) || !clientH || !serverH)
            {
                informarMotor();
                return false;
            }

            uintptr_t serverC = 0;
            if (!ReadPtr(serverH + kOff_InvHolder_Container, &serverC) || serverC < kMinPointer)
                return false;
            const uintptr_t alloc = IdAllocator(serverC);
            if (!alloc)
            {
                informarMotor();
                return false;
            }
            MEMORY_BASIC_INFORMATION paginaContador{};
            const uintptr_t contador = alloc + kOff_IdAlloc_Counter;
            if ((contador & 7) || !VirtualQuery(reinterpret_cast<void*>(contador), &paginaContador, sizeof(paginaContador)) ||
                paginaContador.State != MEM_COMMIT || (paginaContador.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
                (paginaContador.Protect & 0xFF) != PAGE_READWRITE) return false;
            const int64_t id = _InterlockedIncrement64(
                reinterpret_cast<volatile int64_t*>(alloc + kOff_IdAlloc_Counter));
            if (id <= 0 || id == INT64_MAX) { ds::falloParcialInventario(); return false; }

            const bool okServer = AddIntoHolder(serverH, true,  typeId, qty, id, def);
            const bool okClient = okServer && ds::inventarioActivo() && AddIntoHolder(clientH, false, typeId, qty, id, def);

            if (okServer && okClient)
                return true;

            informarMotor();
            if (okServer != okClient) ds::falloParcialInventario();
            return okServer && okClient;
        }

        std::mutex               g_bulkMutex;
        std::vector<AddRequest>  g_bulkQueue; 
        std::atomic<int>         g_bulkTotal{0};
        std::atomic<int>         g_bulkAdded{0};
        std::atomic<int>         g_bulkFailed{0};
        std::atomic<bool>        g_bulkActive{false};

        void RunBulkAdd()
        {
            if (!g_bulkActive.load(std::memory_order_acquire)) return;

            constexpr int kPerTick = 16;
            for (int n = 0; n < kPerTick; ++n)
            {
                AddRequest req;
                {
                    std::lock_guard<std::mutex> lk(g_bulkMutex);
                    if (g_bulkQueue.empty()) break;
                    req = g_bulkQueue.back(); 
                    g_bulkQueue.pop_back();
                }
                if (CommitAdd(req.typeId, req.qty))
                    g_bulkAdded.fetch_add(1, std::memory_order_relaxed);
                else
                    g_bulkFailed.fetch_add(1, std::memory_order_relaxed);
            }

            std::lock_guard<std::mutex> lk(g_bulkMutex);
            if (g_bulkQueue.empty())
                g_bulkActive.store(false, std::memory_order_release); 
        }

        void RunPendingAdd()
        {
            if (g_addPending.load(std::memory_order_acquire))
            {
                const AddRequest req = g_addReq;
                g_addPending.store(false, std::memory_order_release);
                g_addState.store(static_cast<int>(CommitAdd(req.typeId, req.qty)
                                     ? Inventory::AddState::Added
                                     : Inventory::AddState::Failed),
                                 std::memory_order_release);
            }
            RunBulkAdd();
        }
    }

    namespace
    {

        std::vector<Group> g_catalog;
        bool g_catalogBuilt = false;

        void BuildCatalog()
        {
            if (g_catalogBuilt) return;

            if (!g_itemTableGlobal) return;

            uintptr_t table = 0;
            if (!ReadPtr(g_itemTableGlobal, &table)) return;
            uint32_t count = 0;
            if (!Read32(table + kOff_ItemTable_Count, &count) || !count || count > 65536) return;

            g_catalogBuilt = true;
            uint32_t named = 0;
            for (uint32_t row = 0; row < count; ++row)
            {
                const uint16_t tid = static_cast<uint16_t>(row);
                if (tid == kInvSlot_EmptyType) continue;
                uintptr_t def = 0;
                if (!DefForRow(g_itemTableGlobal, tid, &def)) continue;

                Item it{};
                it.typeId = tid;

                if (!DisplayNameForType(tid, it.name, sizeof(it.name)))
                {
                    if (!KeyForType(tid, it.key, sizeof(it.key))) continue;
                    Prettify(it.key, it.name, sizeof(it.name));
                }
                if (!it.name[0]) continue;
                if (!it.key[0] && !KeyForType(tid, it.key, sizeof(it.key))) it.key[0] = 0;
                if (!IconForType(tid, it.icon, sizeof(it.icon))) it.icon[0] = 0;
                if (!CategoryOfType(tid, &it.cat)) it.cat = kNoCategory;
                it.tier = TierOfType(tid);
                ++named;

                Group* g = nullptr;
                for (auto& cand : g_catalog)
                    if (cand.cat.row == it.cat.row) { g = &cand; break; }
                if (!g)
                {
                    Group ng{};
                    ng.cat = it.cat;
                    if (!GroupName(it.cat.row, ng.label, sizeof(ng.label)))
                        snprintf(ng.label, sizeof(ng.label), "Sin categoría");
                    if (it.cat.tabRow == 0xFFFF ||
                        !GroupName(it.cat.tabRow, ng.tab, sizeof(ng.tab)))
                        ng.tab[0] = 0;
                    if (!IconForGroup(it.cat.row, ng.icon, sizeof(ng.icon)))
                        snprintf(ng.icon, sizeof(ng.icon), "%s", kIcon_Uncategorised);
                    g_catalog.push_back(std::move(ng));
                    g = &g_catalog.back();
                }
                g->items.push_back(it);
            }

            for (auto& g : g_catalog)
                std::sort(g.items.begin(), g.items.end(), [](const Item& a, const Item& b) {
                    return _stricmp(a.name, b.name) < 0;
                });
            
            std::sort(g_catalog.begin(), g_catalog.end(), [](const Group& a, const Group& b) {
                if (a.cat.tabOrder != b.cat.tabOrder) return a.cat.tabOrder < b.cat.tabOrder;
                return a.cat.order < b.cat.order;
            });
        }

        Group* CatGroupAt(int cat)
        {
            BuildCatalog();
            return (cat >= 0 && cat < static_cast<int>(g_catalog.size())) ? &g_catalog[cat] : nullptr;
        }
    }

    bool Inventory::CatalogReady()
    {
        BuildCatalog();
        return !g_catalog.empty();
    }

    int Inventory::CatalogCategoryCount()
    {
        BuildCatalog();
        return static_cast<int>(g_catalog.size());
    }

    const char* Inventory::CatalogCategoryName(int cat)
    {
        const Group* g = CatGroupAt(cat);
        return g ? g->label : "";
    }

    const char* Inventory::CatalogCategoryTab(int cat)
    {
        const Group* g = CatGroupAt(cat);
        return g ? g->tab : "";
    }

    const char* Inventory::CatalogCategoryIcon(int cat)
    {
        const Group* g = CatGroupAt(cat);
        return g ? g->icon : "";
    }

    int Inventory::CatalogItemCount(int cat)
    {
        const Group* g = CatGroupAt(cat);
        return g ? static_cast<int>(g->items.size()) : 0;
    }

    bool Inventory::GetCatalogItem(int cat, int idx, ItemInfo* out)
    {
        if (!out) return false;
        const Group* g = CatGroupAt(cat);
        if (!g || idx < 0 || idx >= static_cast<int>(g->items.size())) return false;
        const Item& it = g->items[idx];
        out->name   = it.name;
        out->key    = it.key;
        out->icon   = it.icon;
        out->qty    = 0; 
        out->typeId = it.typeId;
        out->tier   = it.tier;
        return true;
    }

    bool Inventory::AddItem(uint16_t typeId, int64_t qty)
    {
        if (qty < 1) return false;
        if (typeId == kInvSlot_EmptyType) return false;
        uintptr_t def = 0;
        if (!DefForRow(g_itemTableGlobal, typeId, &def)) return false; 
        if (g_addPending.load(std::memory_order_acquire)) return false; 
        g_addReq = { typeId, qty };
        g_addState.store(static_cast<int>(AddState::Pending), std::memory_order_release);
        g_addPending.store(true, std::memory_order_release);
        return true;
    }

    Inventory::AddState Inventory::AddStatus()
    {
        return static_cast<AddState>(g_addState.load(std::memory_order_acquire));
    }

    bool Inventory::AddItemsBulk(const uint16_t* typeIds, int count, int64_t qtyEach)
    {
        if (!typeIds || count <= 0 || qtyEach < 1) return false;
        if (g_bulkActive.load(std::memory_order_acquire)) return false; 

        std::vector<AddRequest> batch;
        batch.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
        {
            const uint16_t tid = typeIds[i];
            if (tid == kInvSlot_EmptyType) continue;
            uintptr_t def = 0;
            if (!DefForRow(g_itemTableGlobal, tid, &def)) continue; 
            batch.push_back({ tid, qtyEach });
        }
        if (batch.empty()) return false;

        const int queued = static_cast<int>(batch.size());
        {
            std::lock_guard<std::mutex> lk(g_bulkMutex);
            g_bulkQueue = std::move(batch);
        }
        g_bulkAdded.store(0, std::memory_order_relaxed);
        g_bulkFailed.store(0, std::memory_order_relaxed);
        g_bulkTotal.store(queued, std::memory_order_release);
        g_bulkActive.store(true, std::memory_order_release); 
        return true;
    }

    Inventory::BulkAdd Inventory::BulkAddStatus()
    {
        BulkAdd s{};
        s.total  = g_bulkTotal.load(std::memory_order_acquire);
        s.added  = g_bulkAdded.load(std::memory_order_acquire);
        s.failed = g_bulkFailed.load(std::memory_order_acquire);
        s.active = g_bulkActive.load(std::memory_order_acquire);
        return s;
    }

    bool Inventory::RemoveItem(int st, int cat, int idx)
    {
        Item* ip = ItemAt(st, cat, idx);
        if (!ip) return false;
        Item& it = *ip;

        if (!Write16(it.slot + kOff_InvSlot_TypeId, kInvSlot_EmptyType)) return false;
        Write64(it.slot + kOff_InvSlot_Quantity, 0);

        const uintptr_t sh = ServerHolder();
        uintptr_t sSlot = SlotByPos(sh, it.bucketIdx, it.slotIdx, it.typeId);
        if (sSlot)
        {
            Write16(sSlot + kOff_InvSlot_TypeId, kInvSlot_EmptyType);
            Write64(sSlot + kOff_InvSlot_Quantity, 0);
        }

        it.qty    = 0;
        it.typeId = kInvSlot_EmptyType; 
        return true;
    }
}

