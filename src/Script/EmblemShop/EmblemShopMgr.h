/*
 * EmblemShopMgr
 *
 * Direct-transaction "buy from emblem vendor" for bots. Bypasses the normal
 * vendor handshake (distance, target, gossip) so a master can spend a remote
 * bot's tokens from anywhere — typically driven by the Botmaster NPC or the
 * .playerbots <bot> buyemblem chat command.
 *
 * Verifies all of: item exists, level / class / race usability, every cost
 * dimension (reqitem[5], honor, arena, personal arena rating), and bag space.
 * Only deducts and stores on success. On failure, errOut is filled with a
 * player-readable reason and nothing changes.
 */

#ifndef _PLAYERBOT_EMBLEM_SHOP_MGR_H
#define _PLAYERBOT_EMBLEM_SHOP_MGR_H

#include "Define.h"

#include <string>

class Player;

class EmblemShopMgr
{
public:
    static EmblemShopMgr& Instance();

    bool BuyForBot(Player* bot, uint32 itemId, uint32 extendedCostId, std::string& errOut);

private:
    EmblemShopMgr() = default;
    EmblemShopMgr(EmblemShopMgr const&) = delete;
    EmblemShopMgr& operator=(EmblemShopMgr const&) = delete;
};

#define sEmblemShopMgr EmblemShopMgr::Instance()

#endif
