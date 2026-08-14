#include "catalog.h"

const CategoryDef *catalog_category(uint16_t category_id) {
    for (size_t i = 0; i < g_category_count; ++i) {
        if (g_categories[i].category_id == category_id) return &g_categories[i];
    }
    return NULL;
}

const ChannelDef *catalog_channel(uint16_t category_id, uint16_t item_id) {
    const CategoryDef *category = catalog_category(category_id);
    if (!category) return NULL;
    for (size_t i = 0; i < category->channel_count; ++i) {
        const ChannelDef *channel = &g_channels[category->first_channel + i];
        if (channel->item_id == item_id) return channel;
    }
    return NULL;
}

const CategoryDef *catalog_category_for_channel(const ChannelDef *channel) {
    return channel ? catalog_category(channel->category_id) : NULL;
}
