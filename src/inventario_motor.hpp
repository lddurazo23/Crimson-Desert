// Interfaz interna adaptada de Trinity; licencia en terceros/trinity.
#pragma once

#include <cstddef>
#include <cstdint>

namespace trinity::game
{

    class Inventory
    {
    public:
        static bool Install();
        static void Remove();

        static bool Ready();

        static void Refresh();

        static void ForceRefresh();

        static int         StorageCount();
        static const char* StorageName(int st);
        static const char* StorageKey(int st);
        static bool        StorageSlots(int st, int* defaultSlots, int* maxSlots);

        static int         StorageItemCount(int st);

        static int         CategoryCount(int st);
        static const char* CategoryName(int st, int cat);
        static int         ItemCount(int st, int cat);

        static const char* CategoryTab(int st, int cat);

        static const char* CategoryIcon(int st, int cat);

        static bool GetItem(int st, int cat, int idx, const char** name, int64_t* qty,
                            const char** icon = nullptr);

        struct ItemInfo
        {
            const char* name;
            const char* key;
            const char* icon;
            int64_t     qty;
            uint16_t    typeId;
            uint8_t     tier;
        };
        static bool GetItemInfo(int st, int cat, int idx, ItemInfo* out);

        static bool SetQuantity(int st, int cat, int idx, int64_t value);

        static bool RemoveItem(int st, int cat, int idx);

        static bool AddItem(uint16_t typeId, int64_t qty);

        enum class AddState { Idle, Pending, Added, Failed };
        static AddState AddStatus();

        static bool AddItemsBulk(const uint16_t* typeIds, int count, int64_t qtyEach);

        struct BulkAdd { int total; int added; int failed; bool active; };
        static BulkAdd BulkAddStatus();

        static int         CatalogCategoryCount();
        static const char* CatalogCategoryName(int cat);
        static const char* CatalogCategoryTab(int cat);
        static const char* CatalogCategoryIcon(int cat);
        static int         CatalogItemCount(int cat);
        static bool        GetCatalogItem(int cat, int idx, ItemInfo* out);

        static bool CatalogReady();

        static bool EditsPersist();

        static bool SetAllMaxStackSizes(bool enable, int64_t value);
        static bool SetAllSlotSizes(bool enable, int value);

        static void Tick();

        static bool NameForTypeId(uint16_t typeId, char* out, size_t n);
        static bool IconForTypeId(uint16_t typeId, char* out, size_t n);

        static uintptr_t ClientCharacterAddr();
        static uintptr_t ServerCharacterAddr();
        static uintptr_t RealmFlagAddress(uint8_t* outVal);
    };
}

