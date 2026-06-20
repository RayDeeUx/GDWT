#include "../hooks/PlayLayer.hpp"

void GDWTPlayLayer::destroyPlayer(PlayerObject* player, GameObject* p1){
    PlayLayer::destroyPlayer(player, p1);

    if (!player->m_isDead) return;

    if (!data::getIsInMatch()) return;

    if (!m_fields->hasRespawned) return;
    m_fields->hasRespawned = false;

    int percent = this->getCurrentPercentInt();

    int combo = data::getCombo(this->m_level->m_levelID.value(), percent);

    if (!(m_fields->currentRunStartprecent == 0 && !m_isPracticeMode)) return;
    if (this->m_level->m_levelID == 0) return;
    
    if (percent <= 5 && !data::getSmallRunsAllowed()) return;

    sendProgressMessage(percent, this->m_level, combo);
}

void GDWTPlayLayer::resetLevel() {
    PlayLayer::resetLevel();

    if (!data::getIsInMatch()) return;

    m_fields->hasRespawned = true;

    m_fields->currentRunStartprecent = this->getCurrentPercent();
}

void GDWTPlayLayer::levelComplete() {
    PlayLayer::levelComplete();

    if (!data::getIsInMatch()) return;

    if (!m_fields->hasRespawned) return;
    m_fields->hasRespawned = false;

    int combo = data::getCombo(this->m_level->m_levelID.value(), this->getCurrentPercentInt());

    if (!(m_fields->currentRunStartprecent == 0 && !m_isPracticeMode)) return;
    if (this->m_level->m_levelID == 0) return;

    sendProgressMessage(100, this->m_level, combo);
}

void GDWTPlayLayer::sendProgressMessage(int precent, GJGameLevel* level, int combo){
    auto embed = data::embedWithPlayerColor();

    DiscordMessage message{};

    embed.title = fmt::format("{} got {}% on {}", GJAccountManager::get()->m_username, precent, level->m_levelName);

    embed.description = fmt::format("LevelID: {} | AccountID: {}", level->m_levelID.value(), GJAccountManager::get()->m_accountID);

    if (combo > 0){
        embed.description += " | Combo: x" + std::to_string(combo + 1);
    }

    message.embeds.push_back(embed);

    m_fields->listener.spawn(
        sendMessagesFuture(message, precent),
        [](Result<> res){

        }
    );
}

arc::Future<Result<>> GDWTPlayLayer::sendMessagesFuture(DiscordMessage message, int precent){
    auto discRes = co_await data::SendDiscordMessage(message);

    auto sheetRes = co_await data::SendSheetProgress(std::to_string(precent));

    std::string errMessage = "Failed to send message! failes: ";
    bool isErr = false;

    if (discRes.isErr()){
        errMessage += fmt::format("discord: {}", discRes.unwrapErr());
        isErr = true;
    }
    if (sheetRes.isErr()){
        errMessage += fmt::format("sheets: {}", sheetRes.unwrapErr());
        isErr = true;
    }

    if (isErr)
        co_return Err(errMessage);
    
    co_return Ok();
}