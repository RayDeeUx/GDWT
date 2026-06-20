#include "../utils/data.hpp"
#include <Geode/utils/web.hpp>

// == static members ==

std::string data::SheetID = "1D-x1ABxhvJHb6rQ1T0LC0TF3MOeaEXLSDxwRhuMk9nE";

std::string data::matchesPageID = "Match%20Raw%20Data";

std::string data::playersPageID = "Player%20Raw%20Data";

std::string data::matchGroupsPageID = "Match%20Groups%20Data";

std::vector<Match> data::loadedMatches{};

std::vector<GDWTUserInfo> data::loadedUsersInfo{};

std::vector<Team> data::loadedTeams{};

std::vector<PlayerData> data::LoadedPlayers{};

std::vector<GJGameLevel*> data::loadedLevels{};

std::vector<MatchGroup> data::LoadedMatchGroups{};

Ref<CCDictionary> data::loadedImages = CCDictionary::create();

std::tuple<int, int, int> data::lastLevelProgress{};

bool data::isInMatch = false;

std::string data::discordWebhookLink = "https://discord.com/api/webhooks/";

std::string data::discordWebhookSecret = "";

std::string data::sheetsClientID = "";
std::string data::sheetsClientSecret = "";
std::string data::sheetsRefreshToken = "";

bool data::CBFAllowed = false;

bool data::discordConnectionCheck = false;
bool data::sheetsConnectionCheck = false;
bool data::connecting = false;
std::string data::matchSheetID = "";
std::string data::matchSheetName = "";
CCPoint data::columnsMinMax = ccp(0, 0);
std::string data::connectCheckCell = "";

SEL_CallFuncO data::m_callback;
CCNode* data::m_target = nullptr;

// == functions ==

MatchesFuture data::getMatchesData(){
    if (loadedMatches.size()){
        co_return Ok(data::loadedMatches);
    }

    web::WebRequest req = web::WebRequest();

    //req.param("key", apiKey);

    auto res = co_await req.get(fmt::format("https://docs.google.com/spreadsheets/d/{}/gviz/tq?tqx=out:csv&sheet={}", SheetID, matchesPageID));

    GEODE_CO_UNWRAP_INTO(auto sheetString, res.string());

    auto matches = std::vector<Match>{};

    if (sheetString.empty()){
        co_return Err("failed to get matches");
    }

    std::vector<std::vector<std::string>> values = convertRawData(sheetString, true);
                
    for (int r = 0; r < values.size(); r++)
    {
        if (values[r].size() - 1 != 9) continue;
        
        Match currMatch;

        currMatch.matchName = values[r][7];
        
        auto hosts = splitStr(values[r][0], ";");
        
        for (int i = 0; i < hosts.size(); i++)
        {
            if (hosts[i] == "NA")
                break;
            Host h;
            auto splittedHost = splitStr(hosts[i], ",");
            h.displayName = splittedHost[0];
            h.accountID = geode::utils::numFromString<int>(splittedHost[1]).unwrapOr(-1);

            currMatch.hosts.push_back(h);
        }
        
        auto coHosts = splitStr(values[r][1], ";");
        
        for (int i = 0; i < coHosts.size(); i++)
        {
            if (coHosts[i] == "NA")
                break;
            Host coh;
            auto splittedcoHost = splitStr(coHosts[i], ",");
            coh.displayName = splittedcoHost[0];
            coh.accountID = geode::utils::numFromString<int>(splittedcoHost[1]).unwrapOr(-1);

            currMatch.coHosts.push_back(coh);
        }

        currMatch.dateUnix = geode::utils::numFromString<long long>(values[r][2]).unwrapOr(-1);

        time_t time = currMatch.dateUnix;
        auto tm = std::localtime(&time);
        currMatch.date = fmt::format("{}/{}/{}", tm->tm_mon + 1, tm->tm_mday, tm->tm_year + 1900);

        currMatch.teams = splitStr(values[r][3], ",");
        auto splittedLevels = splitStr(values[r][4], ";");
        
        if (values[r][4] == "NA" || values[r][4].empty())
            splittedLevels.clear();

        for (int i = 0; i < splittedLevels.size(); i++)
        {
            auto splittedLevel = splitStr(splittedLevels[i], ",");
            Level level;
            
            auto splittedLevelStats = splitStr(splittedLevel[0], "-");

            level.levelID = geode::utils::numFromString<int>(splittedLevelStats[0]).unwrapOr(-1);
            if (splittedLevelStats.size() > 1)
                level.playTime = geode::utils::numFromString<int>(splittedLevelStats[1]).unwrapOr(-1);
            if (splittedLevel.size() > 1)
                for (int d = 1; d < splittedLevel.size(); d++)
                {
                    auto pdata = splitStr(splittedLevel[d], "-");

                    auto pdata1 = geode::utils::numFromString<int>(pdata[0]);
                    auto pdata2 = geode::utils::numFromString<int>(pdata[1]);

                    if (pdata1.isOk() && pdata2.isOk()){
                        auto toAdd = std::tuple<int, int, std::string>{pdata1.unwrap(), 0, "default"};

                        if (pdata.size() > 1){
                            std::get<1>(toAdd) = geode::utils::numFromString<int>(pdata[1]).unwrapOr(-1);
                        }
                        if (pdata.size() > 2){
                            std::get<2>(toAdd) = pdata2.unwrap();
                        }
                        level.accountIDs.push_back(toAdd);
                    }
                    else {
                        auto toAdd = std::tuple<std::string, std::string, int>{pdata[0], pdata[1], 0};

                        if (pdata.size() > 2){
                            std::get<2>(toAdd) = geode::utils::numFromString<int>(pdata[2]).unwrapOr(-1);
                        }

                        if (pdata[0].find(' ') != std::string::npos)
                            log::warn("Account ID for player {} in match \"{}\" contains a space!", pdata[0], currMatch.matchName);
                        
                        level.displayNames.push_back(toAdd);
                    }


                }
            currMatch.levels.push_back(level);
        }

        currMatch.liveLink = values[r][5];
        currMatch.vodLink = values[r][6];
        
        currMatch.scoreType = static_cast<ScoreSystemType>(geode::utils::numFromString<int>(values[r][8]).unwrapOr(0));
        currMatch.groupID = geode::utils::numFromString<int>(values[r][9]).unwrapOr(-1);

        matches.push_back(currMatch);
    }

    std::ranges::sort(matches, [](const Match a, const Match b) {
        return b.dateUnix < a.dateUnix;
    });
    
    loadMatches(matches);

    co_return Ok(matches);
}

TeamsFuture data::getTeamsData(){
    if (loadedTeams.size()){
        co_return Ok(data::loadedTeams);
    }

    web::WebRequest req = web::WebRequest();

    req.param("majorDimension", "COLUMNS");

    auto pDataRes = co_await getPlayersData();

    auto players = pDataRes.unwrapOrDefault();

    if (!players.size()){
        if (pDataRes.isErr())
            data::sendError(pDataRes.unwrapErr());
        co_return Err("failed to get teams");
    }

    std::vector<Team> teams{};

    for (int p = 0; p < players.size(); p++)
    {
        if (players[p].countryCode == "N/A") continue;

        bool doesnMyTeamExist = false;

        for (int i = 0; i < teams.size(); i++)
        {
            if (teams[i].countryCode == players[p].countryCode){
                doesnMyTeamExist = true;
                teams[i].accounts.push_back(players[p]);
                break;
            }
        }

        if (doesnMyTeamExist){
            continue;
        }
        else{
            Team team;
            team.countryCode = players[p].countryCode;
            team.regionCode = players[p].regionCode;
            team.accounts.push_back(players[p]);
            teams.push_back(team);
        }
    }

    std::ranges::sort(teams, [](const Team& a, const Team& b) {
        if (a.regionCode == b.regionCode) {
            return a.countryCode < b.countryCode; 
        }

        if (a.regionCode == "A") return true;
        if (b.regionCode == "A") return false;

        if (a.regionCode == "EU") return true;
        if (b.regionCode == "EU") return false;

        return a.regionCode < b.regionCode;
    });
    
    loadTeams(teams);
    
    co_return Ok(teams);
}

std::vector<std::string> data::splitStr(std::string str, std::string delim) {
    size_t posStart = 0;
    size_t posEnd;
    size_t delimLen = delim.length();

    std::vector<std::string> res{};

    while ((posEnd = str.find(delim, posStart)) != std::string::npos) {
        std::string token = str.substr(posStart, posEnd - posStart);
        posStart = posEnd + delimLen;
        if (token != ""){
            if (token[0] == '\"' && token[token.length() - 1] == '\"' && token.length() > 1){
                token = token.erase(0, 1);
                token = token.erase(token.length() - 1, 1);
            }  

            res.push_back(token);
        }
    }

    auto ps = str.substr(posStart);

    if (ps[0] == '\"' && ps[ps.length() - 1] == '\"' && ps.length() > 1){
        ps = ps.erase(0, 1);
        ps = ps.erase(ps.length() - 1, 1);
    } 

    res.push_back(ps);
    return res;
}

std::vector<std::string> data::eraseEmptys(std::vector<std::string> array){
    for (int i = 0; i < array.size(); i++)
    {
        if (array[i].empty()){
            array.erase(std::next(array.begin(), i));
            i--;
        }
    }
    return array;
}

std::vector<std::vector<std::string>> data::convertRawData(std::string data, bool rows){
    std::vector<std::vector<std::string>> values{};

    auto lines = splitStr(data, "\n");
    
    for (int i = rows; i < lines.size(); i++)
    {
        auto vals = splitStr(lines[i], "\",");

        if (rows){
            for (int b = 0; b < vals.size(); b++)
            {   
                if (vals[b] != "")
                    if (vals[b][0] == '\"'){
                        vals[b].erase(0, 1);
                    }
            }
                    
            vals = eraseEmptys(vals);

            values.push_back(vals);
        }
        else{
            for (int v = 0; v < vals.size(); v++)
            {
                if (i == 0){
                    values.push_back(std::vector<std::string>{vals[v]});
                }
                else{
                    values[v].push_back(vals[v]);
                }
            }
            for (int v1 = 0; v1 < values.size(); v1++)
            {
                for (int v2 = 0; v2 < values[v1].size(); v2++)
                {   
                    if (values[v1][v2] != "")
                        if (values[v1][v2][0] == '\"'){
                            values[v1][v2].erase(0, 1);
                        }
                }
                    
                values[v1] = eraseEmptys(values[v1]);
            }
        }
    }

    return values;         
}

GDWTUserInfoFuture data::getUsersInfo(std::vector<int> userIDs){

    std::vector<GDWTUserInfo> alreadyLoadedUsers{};

    for (int i = 0; i < userIDs.size(); i++)
    {
        GDWTUserInfo user;

        bool doesExist = false;
        for (int u = 0; u < loadedUsersInfo.size(); u++)
        {
            if (loadedUsersInfo[u].accountID == userIDs[i]){
                doesExist = true;
                user = loadedUsersInfo[u];
            }
        }
        
        if (doesExist){
            alreadyLoadedUsers.push_back(user);
            userIDs.erase(std::next(userIDs.begin(), i));
            i--;
        }
    }

    std::vector<GDWTUserInfo> usersToReturn{};

    for (int i = 0; i < userIDs.size(); i++)
    {
        if (userIDs[i] != -1){
            web::WebRequest req = web::WebRequest();

            req.header("Content-Type", "application/x-www-form-urlencoded");

            req.userAgent("");

            req.bodyString(fmt::format("secret=Wmfd2893gb7&targetAccountID={}", userIDs[i]));

            auto res = co_await req.post("http://www.boomlings.com/database/getGJUserInfo20.php");

            GDWTUserInfo errUser{};
            errUser.accountID = -1;

            auto userInfoString = res.string().unwrapOr("-1");

            if (userInfoString == "-1"){
                usersToReturn.push_back(errUser);
                continue;
            }
            auto InfoRes = data::parseUserInfo(userInfoString);
            if (!InfoRes.isOk()){
                usersToReturn.push_back(errUser);
                continue;
            }

            data::loadUserInfo(InfoRes.unwrap());

            usersToReturn.push_back(InfoRes.unwrap());
        }
        
    }

    for (int i = 0; i < alreadyLoadedUsers.size(); i++)
    {
        GDWTUserInfo info = alreadyLoadedUsers[i];

        usersToReturn.push_back(info);
    }
    

    co_return usersToReturn;
}

Result<GDWTUserInfo> data::parseUserInfo(std::string infoRaw){
    if (infoRaw == "-1")
        return Err("invalid string!");

    if (infoRaw.length() >= 5){
        if (infoRaw[0] == 'e' && infoRaw[1] == 'r' && infoRaw[2] == 'r' && infoRaw[3] == 'o' && infoRaw[4] == 'r'){
            return Err("invalid string!");
        }
    }    

    std::map<int, std::string> infoMap{};

    bool isKey = true;

    std::pair<int, std::string> currentElement{};

    std::string currThing = "";

    //log::info("r {}", infoRaw);

    for (int i = 0; i < infoRaw.length(); i++)
    {
        if (infoRaw[i] == ':' || infoRaw.length() == i + 1){
            if (infoRaw.length() == i + 1 && infoRaw[i] != ':')
                currThing += infoRaw[i];
            //log::info("registering (key = {})", isKey);
            if (isKey){
                GEODE_UNWRAP_INTO(currentElement.first, geode::utils::numFromString<int>(currThing));
                //log::info("added '{}' into the key", currThing);
            }
            else{
                currentElement.second = currThing;
                //log::info("added '{}' into the value of key {}", currThing, currentElement.first);

                infoMap.insert(currentElement);
                //log::info("inserted {}-{}", currentElement.first, currentElement.second);
            }

            isKey = !isKey;
            currThing = "";
        }
        else{
            //log::info("added '{}'", infoRaw[i]);
            currThing += infoRaw[i];
        }
    }

    GDWTUserInfo info;
    
    info.userName = infoMap[1];
    GEODE_UNWRAP_INTO(info.userID, geode::utils::numFromString<int>(infoMap[2]));
    GEODE_UNWRAP_INTO(info.stars, geode::utils::numFromString<int>(infoMap[3]));
    GEODE_UNWRAP_INTO(info.demons, geode::utils::numFromString<int>(infoMap[4]));
    GEODE_UNWRAP_INTO(info.creatorPoints, geode::utils::numFromString<int>(infoMap[8]));
    GEODE_UNWRAP_INTO(info.playerColor, geode::utils::numFromString<int>(infoMap[10]));
    GEODE_UNWRAP_INTO(info.playerColor2, geode::utils::numFromString<int>(infoMap[11]));
    GEODE_UNWRAP_INTO(info.secretCoins, geode::utils::numFromString<int>(infoMap[13]));
    GEODE_UNWRAP_INTO(info.accountID, geode::utils::numFromString<int>(infoMap[16]));
    GEODE_UNWRAP_INTO(info.userCoins, geode::utils::numFromString<int>(infoMap[17]));
    GEODE_UNWRAP_INTO(info.messageState, geode::utils::numFromString<int>(infoMap[18]));
    GEODE_UNWRAP_INTO(info.friendState, geode::utils::numFromString<int>(infoMap[19]));
    info.youtubeLink = infoMap[20];
    GEODE_UNWRAP_INTO(info.accIcon, geode::utils::numFromString<int>(infoMap[21]));
    GEODE_UNWRAP_INTO(info.accShip, geode::utils::numFromString<int>(infoMap[22]));
    GEODE_UNWRAP_INTO(info.accBall, geode::utils::numFromString<int>(infoMap[23]));
    GEODE_UNWRAP_INTO(info.accUfo, geode::utils::numFromString<int>(infoMap[24]));
    GEODE_UNWRAP_INTO(info.accWave, geode::utils::numFromString<int>(infoMap[25]));
    GEODE_UNWRAP_INTO(info.accRobot, geode::utils::numFromString<int>(infoMap[26]));
    GEODE_UNWRAP_INTO(info.accGlow, geode::utils::numFromString<int>(infoMap[28]));
    GEODE_UNWRAP_INTO(info.isRegistered, geode::utils::numFromString<int>(infoMap[29]));
    GEODE_UNWRAP_INTO(info.globalRank, geode::utils::numFromString<int>(infoMap[30]));
    GEODE_UNWRAP_INTO(info.friendstate, geode::utils::numFromString<int>(infoMap[31]));
    GEODE_UNWRAP_INTO(info.accSpider, geode::utils::numFromString<int>(infoMap[43]));
    info.twitterLink = infoMap[44];
    info.twitch = infoMap[45];
    GEODE_UNWRAP_INTO(info.diamonds, geode::utils::numFromString<int>(infoMap[46]));
    GEODE_UNWRAP_INTO(info.accExplosion, geode::utils::numFromString<int>(infoMap[48]));
    GEODE_UNWRAP_INTO(info.modLevel, geode::utils::numFromString<int>(infoMap[49]));
    GEODE_UNWRAP_INTO(info.commentHistoryState, geode::utils::numFromString<int>(infoMap[50]));
    GEODE_UNWRAP_INTO(info.glowColor, geode::utils::numFromString<int>(infoMap[51]));
    GEODE_UNWRAP_INTO(info.moons, geode::utils::numFromString<int>(infoMap[52]));
    info.demonCategories = infoMap[55];
    info.normalLevelCategories = infoMap[56];
    info.platformerLevelCategories = infoMap[57];

    return Ok(info);

}

void data::loadMatches(const std::vector<Match>& match){
    loadedMatches.clear();
    loadedMatches = match;
}

void data::loadUserInfo(GDWTUserInfo& user){
    bool doesExist = false;
    for (int u = 0; u < loadedUsersInfo.size(); u++)
    {
        if (loadedUsersInfo[u].accountID == user.accountID)
            doesExist = true;
    }

    if (doesExist) return;
    
    loadedUsersInfo.insert(loadedUsersInfo.begin(), user);
}

void data::loadTeams(const std::vector<Team>& teams){
    loadedTeams.clear();
    loadedTeams = teams;
}

GJGameLevel* data::getLoadedLevelByID(int level){
    GJGameLevel* toReturn = nullptr;
    for (int l = 0; l < loadedLevels.size(); l++)
    {
        if (loadedLevels[l]->m_levelID.value() == level)
            toReturn = loadedLevels[l];
    }

    return toReturn;
}

void data::loadLevel(GJGameLevel* level){
    bool doesExist = false;
    for (int l = 0; l < loadedLevels.size(); l++)
    {
        if (loadedLevels[l]->m_levelID.value() == level->m_levelID.value())
            doesExist = true;
    }

    if (doesExist) return;
    
    loadedLevels.push_back(level);
}

PlayerDataFuture data::getPlayersData(){
    if (LoadedPlayers.size()){
        co_return Ok(data::LoadedPlayers);
    }

    web::WebRequest req = web::WebRequest();

    //req.param("key", apiKey);

    auto res = co_await req.get(fmt::format("https://docs.google.com/spreadsheets/d/{}/gviz/tq?tqx=out:csv&sheet={}", SheetID, playersPageID));

    GEODE_CO_UNWRAP_INTO(auto resString, res.string());

    auto players = std::vector<PlayerData>{};

    if (resString.empty()){
        co_return Err("failed to get players");
    }

    std::vector<std::vector<std::string>> values = convertRawData(resString, true);
    
    for (int r = 0; r < values.size(); r++)
    {
        if (values[r].size() - 1 != 7) continue;

        PlayerData currPlayer;

        currPlayer.countryCode = values[r][0];
        currPlayer.regionCode = values[r][1];

        currPlayer.accountID = geode::utils::numFromString<int>(values[r][2]).unwrapOr(-1);
        currPlayer.displayName = values[r][3];

        auto iconInfo = splitStr(values[r][4], ",");
        
        if (iconInfo.size() == 5){
            currPlayer.iconID = geode::utils::numFromString<int>(iconInfo[0]).unwrapOr(-1);
            currPlayer.color1ID = geode::utils::numFromString<int>(iconInfo[1]).unwrapOr(-1);
            currPlayer.color2ID = geode::utils::numFromString<int>(iconInfo[2]).unwrapOr(-1);
            currPlayer.glowEnabled = iconInfo[3] == "1";
            currPlayer.glowColorID = geode::utils::numFromString<int>(iconInfo[4]).unwrapOr(-1);
        }
        
        currPlayer.isActive = values[r][5] != "1";

        auto staffIDs = splitStr(values[r][6], ",");

        for (const auto& ID : staffIDs)
        {
            currPlayer.staffIDs.emplace(ID);
        }

        auto AchievementIDs = splitStr(values[r][7], ",");

        for (const auto& ID : AchievementIDs)
        {
            currPlayer.achievementIDs.emplace(ID);
        }

        players.push_back(currPlayer);
    }
    
    LoadedPlayers.clear();
    LoadedPlayers = players;

    co_return Ok(players);
}

void data::addImage(CCImage* image, std::string ID){
    loadedImages->setObject(image, ID);
}

CCImage* data::getImage(std::string ID){
    return dynamic_cast<CCImage*>(loadedImages->objectForKey(ID));
}

scoreCalcFuture data::calculateScores(std::vector<Level> levels, std::vector<std::string> expectedTeams, ScoreSystemType type){
    auto _teams = co_await getTeamsData();

    GEODE_CO_UNWRAP_INTO(auto teams, _teams);

    std::vector<std::tuple<std::string, int, int>> toReturn{};

    std::map<std::string, std::pair<int, int>> overallScores{};

    for (int i = 0; i < levels.size(); i++)
    {
        //get level players teams
        std::vector<std::tuple<std::string, int>> players{};
        for (int p = 0; p < levels[i].accountIDs.size(); p++)
        {
            std::string cc = "NA";
            for (int t = 0; t < teams.size(); t++){
                if (teams[t].countryCode == std::get<2>(levels[i].accountIDs[p])){
                    cc = teams[t].countryCode;
                    std::transform(cc.begin(), cc.end(), cc.begin(), ::tolower);
                    break;
                }
                
                for (int ac = 0; ac < teams[t].accounts.size(); ac++){
                    if (teams[t].accounts[ac].accountID == std::get<0>(levels[i].accountIDs[p])){
                        cc = teams[t].countryCode;
                        std::transform(cc.begin(), cc.end(), cc.begin(), ::tolower);
                    }
                }
            }

            int currScore = std::get<1>(levels[i].accountIDs[p]);

            players.push_back(std::tuple<std::string, int>{cc, currScore});
        }
        
        for (int p = 0; p < levels[i].displayNames.size(); p++)
        {
            std::string cc = std::get<1>(levels[i].displayNames[p]);
            std::transform(cc.begin(), cc.end(), cc.begin(), ::tolower);

            int currScore = std::get<2>(levels[i].displayNames[p]);

            for (int t = 0; t < teams.size(); t++){
                bool hasFound = false;
                for (int ac = 0; ac < teams[t].accounts.size(); ac++){
                    if (teams[t].accounts[ac].accountID == std::get<0>(levels[i].accountIDs[p])){
                        cc = teams[t].countryCode;
                        std::transform(cc.begin(), cc.end(), cc.begin(), ::tolower);
                    }
                }
            }

            players.push_back(std::tuple<std::string, int>{cc, currScore});
        }

        //add missing teams
        for (int t = 0; t < expectedTeams.size(); t++)
        {
            std::string lowerTeamCCode = expectedTeams[t];
            std::transform(lowerTeamCCode.begin(), lowerTeamCCode.end(), lowerTeamCCode.begin(), ::tolower);

            bool doesTeamExist = false;
            for (int p = 0; p < players.size(); p++)
            {
                if (std::get<0>(players[p]) == lowerTeamCCode)
                    doesTeamExist = true;
            }
            
            if (!doesTeamExist){
                players.push_back(std::tuple<std::string, int>{lowerTeamCCode, 0});
            }
        }
        
        //sort by placement
        std::ranges::sort(players, [](const std::tuple<std::string, int> a, const std::tuple<std::string, int> b) {
            auto scoreA = std::get<1>(a);
            auto scoreB = std::get<1>(b);

            return scoreA < scoreB;
        });
        
        //calc windraw
        if (type == ScoreSystemType::WinDraw || type == ScoreSystemType::WinDrawT2 || type == ScoreSystemType::WinDrawTimes2){
            //get a list of all ties in the match
            int currHighest = 0;
            std::map<int, std::vector<std::pair<int, int>>> tiedScores;
            for (int p = 0; p < players.size(); p++)
            {
                int score = p;
                if (type == ScoreSystemType::WinDrawTimes2)
                    score *= 2;

                if (currHighest == std::get<1>(players[p]) && currHighest != 0){
                    if (tiedScores.contains(currHighest))
                        tiedScores[currHighest].push_back(std::pair<int, int>{score, std::get<1>(players[p])});
                    else
                        tiedScores.insert(std::pair<int, std::vector<std::pair<int, int>>>{currHighest, std::vector<std::pair<int, int>>{{score, std::get<1>(players[p])}}});
                }

                currHighest = std::get<1>(players[p]);
            }

            //calc
            for (int placement = 0; placement < players.size(); placement++)
            {
                std::string keyCC = std::get<0>(players[placement]);
                int score = 0;

                score = placement;
                if (type == ScoreSystemType::WinDrawTimes2)
                    score *= 2;

                int playerScore = std::get<1>(players[placement]);

                if (tiedScores.contains(playerScore)){
                    int tiePoints = 0;
                    for (int p = 0; p < tiedScores[playerScore].size(); p++)
                    {
                        tiePoints += tiedScores[playerScore][p].first;
                    }
                    score = tiePoints / tiedScores[playerScore].size();
                }

                bool found = false;

                for (int c = 0; c < toReturn.size(); c++)
                {
                    if (std::get<0>(toReturn[c]) == keyCC){
                        std::get<1>(toReturn[c]) += score;
                        std::get<2>(toReturn[c]) += playerScore;
                        found = true;
                    }
                }

                if (!found)
                    toReturn.push_back(std::tuple<std::string, int, int>{keyCC, score, playerScore});
            }

        }
        //calc addition
        else if (type == ScoreSystemType::Addition || type == ScoreSystemType::AdditionT2){
            for (int placement = 0; placement < players.size(); placement++)
            {
                std::string keyCC = std::get<0>(players[placement]);
                int score = std::get<1>(players[placement]);

                bool found = false;

                for (int c = 0; c < toReturn.size(); c++)
                {
                    if (std::get<0>(toReturn[c]) == keyCC){
                        std::get<1>(toReturn[c]) += score;
                        std::get<2>(toReturn[c]) += score;
                        found = true;
                    }
                }

                if (!found)
                    toReturn.push_back(std::tuple<std::string, int, int>{keyCC, score, score});
            }
        }
    }

    std::ranges::sort(toReturn, [](const std::tuple<std::string, int, int> a, const std::tuple<std::string, int, int> b) {
        auto A = std::get<1>(a);
        auto B = std::get<1>(b);

        if (A == B) return std::get<2>(a) < std::get<2>(b);
        return A < B;
    });

    co_return Ok(toReturn);
}

CCNode* data::createCircleGlow(ccColor3B color, float opacity){
    auto node = CCNode::create();

    auto glow1 = CCSprite::createWithSpriteFrameName("d_gradient_c_02_001.png");
    glow1->setAnchorPoint({1, 0});
    glow1->setColor(color);
    glow1->setOpacity(opacity);
    node->addChild(glow1);

    auto glow2 = CCSprite::createWithSpriteFrameName("d_gradient_c_02_001.png");
    glow2->setAnchorPoint({1, 0});
    glow2->setRotation(90);
    glow2->setColor(color);
    glow2->setOpacity(opacity);
    node->addChild(glow2);

    auto glow3 = CCSprite::createWithSpriteFrameName("d_gradient_c_02_001.png");
    glow3->setAnchorPoint({1, 0});
    glow3->setRotation(180);
    glow3->setColor(color);
    glow3->setOpacity(opacity);
    node->addChild(glow3);

    auto glow4 = CCSprite::createWithSpriteFrameName("d_gradient_c_02_001.png");
    glow4->setAnchorPoint({1, 0});
    glow4->setRotation(270);
    glow4->setColor(color);
    glow4->setOpacity(opacity);
    node->addChild(glow4);

    return node;
}

std::string data::ScoreSystemTypeToString(ScoreSystemType type){
    switch (type)
    {
        case ScoreSystemType::Addition:
            return "Addition";

        case ScoreSystemType::WinDraw:
            return "Win Draw";

        case ScoreSystemType::AdditionT2:
            return "Addition Top 2";

        case ScoreSystemType::WinDrawT2:
            return "Win Draw Top 2";

        case ScoreSystemType::WinDrawTimes2:
            return "Win Draw Times 2";

        default:
            return "";
    }
}

MatchGroupsDataFuture data::getMatchGroupsData(){
    if (LoadedPlayers.size()){
        co_return Ok(data::LoadedMatchGroups);
    }

    web::WebRequest req = web::WebRequest();

    //req.param("key", apiKey);

    auto res = co_await req.get(fmt::format("https://docs.google.com/spreadsheets/d/{}/gviz/tq?tqx=out:csv&sheet={}", SheetID, matchGroupsPageID));

    GEODE_CO_UNWRAP_INTO(auto MGString, res.string());

    auto groups = std::vector<MatchGroup>{};

    if (MGString.empty()){
        co_return Err("failed getting match groups");
    }

    std::vector<std::vector<std::string>> values = convertRawData(MGString, true);
    
    for (int r = 0; r < values.size(); r++)
    {
        if (values[r].size() - 1 != 3) continue;

        MatchGroup currMGroups;
        
        currMGroups.groupID = geode::utils::numFromString<int>(values[r][0]).unwrapOr(-1);
        currMGroups.groupName = values[r][1];

        currMGroups.amountPass = geode::utils::numFromString<int>(values[r][2]).unwrapOr(-1);

        currMGroups.scoreType = static_cast<ScoreSystemType>(geode::utils::numFromString<int>(values[r][3]).unwrapOr(0));

        groups.push_back(currMGroups);
    }
    
    LoadedMatchGroups.clear();
    LoadedMatchGroups = groups;

    co_return Ok(groups);
}

CurrentMatchFuture data::getCurrentMatchData(std::string accessToken){
    web::WebRequest req = web::WebRequest();

    req.header("Authorization", "Bearer " + accessToken);

    auto res = co_await req.get(fmt::format("https://sheets.googleapis.com/v4/spreadsheets/{}/values/{}?majorDimension=COLUMNS", matchSheetID, matchSheetName));

    GEODE_CO_UNWRAP_INTO(auto json, res.json());
    
    auto matchPlayers = std::map<std::string, std::pair<std::string, std::vector<std::string>>>{};

    GEODE_CO_UNWRAP_INTO(auto vals, json.as<sheetValues>());

    if (vals.range.empty())
        co_return Err("Failed to fetch sheet data!");

    std::vector<std::vector<std::string>> values = vals.values;

    for (int r = columnsMinMax.x; r < values.size(); r++)
    {
        if (columnsMinMax.y < r) break;
        if (!values[r].size()) continue;

        if (values[r][1] == "-1") continue;
        if (matchPlayers.contains(values[r][1])) continue;

        matchPlayers.insert(std::pair<std::string, std::pair<std::string, std::vector<std::string>>>{values[r][1], std::pair<std::string, std::vector<std::string>>{data::columnNumberToLetter(r), std::vector<std::string>{}}});

        for (int k = 1; k < values[r].size(); k++)
        {
            matchPlayers[values[r][1]].second.push_back(values[r][k]);
        }
    }

    if (!matchPlayers.size()) co_return Err("No match players found!");

    co_return Ok(matchPlayers);
}

Result<std::tuple<int, int, int>> data::splitDate(std::string date){
    auto splittedDate = splitStr(date, "/");

    GEODE_UNWRAP_INTO(auto day, geode::utils::numFromString<int>(splittedDate[0]));

    GEODE_UNWRAP_INTO(auto month, geode::utils::numFromString<int>(splittedDate[1]));

    GEODE_UNWRAP_INTO(auto year, geode::utils::numFromString<int>(splittedDate[2]));

    return Ok(std::make_tuple(day, month, year));
}

uint64_t data::getKey() {
    using namespace std::chrono;
    auto now = system_clock::now();
    uint64_t a = time_point_cast<seconds>(now).time_since_epoch().count();
    a = a / 600;
    return a;
}

std::vector<unsigned char> data::base64Decode(const std::string& input) {
    std::vector<unsigned char> decodedBytes;
    
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) {
        T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
    }

    int val = 0;
    int valb = -8;
    
    for (unsigned char c : input) {
        if (T[c] == -1) break;
        
        val = ((val << 6) & 0xFFFFFF) + T[c];
        valb += 6;
        
        if (valb >= 0) {
            decodedBytes.push_back(static_cast<unsigned char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return decodedBytes;
}

// Function to decrypt a string
std::string data::decryptString(const std::string& encryptedInput) {
    uint64_t key = getKey();
    std::vector<unsigned char> encryptedBytes = base64Decode(encryptedInput);

    for (size_t i = 0; i < encryptedBytes.size(); ++i) {
        encryptedBytes[i] ^= static_cast<unsigned char>(key);
    }

    return std::string(encryptedBytes.begin(), encryptedBytes.end());
}

bool data::getIsInMatch() { return isInMatch; }

void data::leaveMatch(){
    web::WebRequest req = web::WebRequest();
    DiscordMessage message{};

    DiscordEmbed e = embedWithPlayerColor();
    e.title = fmt::format("{} Disconnected.", GJAccountManager::get()->m_username);
    e.description = fmt::format("AccountID: {}", std::to_string(GJAccountManager::get()->m_accountID));

    message.embeds.push_back(e);

    auto j = matjson::Value(message);

    req.bodyJSON(j);

    async::spawn(
        req.post(discordWebhookLink + discordWebhookSecret)
    );

    isInMatch = false;
    CBFAllowed = false;
    sheetsConnectionCheck = false;
    discordConnectionCheck = false;
    discordWebhookSecret = "";
    sheetsClientID = "";
    sheetsClientSecret = "";
    sheetsRefreshToken = "";
}

Result<> data::joinMatch(std::string joinCode){
    using namespace std::chrono;
    auto now = system_clock::now();
    long long a = time_point_cast<seconds>(now).time_since_epoch().count();
    a = a / 100;

    auto decrypt = decryptString(joinCode);

    auto secrets = splitStr(decrypt, "|");

    if (secrets.size() != 11){
        return Err("Invalid code!");
    }

    std::string val = fmt::format("{}/{}", secrets[0], secrets[1]);

    std::string stringForCBF = secrets[2];
    
    auto numForCBF = utils::numFromString<int>(stringForCBF).unwrapOr(-1);

    if (numForCBF == -1){
        return Err("Invalid code!");
    }
    else if (numForCBF == 0){
        CBFAllowed = false;
    }
    else{
        CBFAllowed = true;
    }

    sheetsClientID = secrets[3];
    sheetsClientSecret = secrets[4];
    sheetsRefreshToken = secrets[5];

    matchSheetID = secrets[6];
    matchSheetName = secrets[7];
    columnsMinMax.x = utils::numFromString<int>(secrets[8]).unwrapOr(-1);
    columnsMinMax.y = utils::numFromString<int>(secrets[9]).unwrapOr(-1);
    connectCheckCell = secrets[10];

    DiscordMessage message{};

    DiscordEmbed e = embedWithPlayerColor();
    e.title = fmt::format("{} Successfully connected!", GJAccountManager::get()->m_username);
    e.description = fmt::format("AccountID: {}", std::to_string(GJAccountManager::get()->m_accountID));

    message.embeds.push_back(e);

    auto j = matjson::Value(message);

    connecting = true;

    arc::spawn(
        []() -> arc::Future<> {
            auto res = co_await data::refreshAccessToken(sheetsClientID, sheetsClientSecret, sheetsRefreshToken);

            if (res.isErr())
            {
                data::checkConnectionComplete("Incorrect code! (might be expired)");
                co_return;
            }

            auto didWrite = co_await data::writeToGoogleSheet(matchSheetID, fmt::format("{}!{}:{}", matchSheetName, connectCheckCell, connectCheckCell), ":D", res.unwrap());
            
            if (didWrite.isErr()){
                data::checkConnectionComplete("Connection Failed! " + didWrite.unwrapErr());
                co_return;
            }

            data::sheetsConnectionCheck = true;
            data::checkConnectionComplete();
        }
    );

    arc::spawn(
        [&, val, j]() -> arc::Future<> {
            web::WebRequest req = web::WebRequest();
            req.bodyJSON(j);
            auto request = req.post(discordWebhookLink + val);

            auto res = co_await request;

            geode::queueInMainThread([res, val](){
                if (!res.ok())
                data::checkConnectionComplete("Connection Failed! (discord response is error)");

                auto json = res.json();

                if (json.isOk())
                    data::checkConnectionComplete("Incorrect code! " + json.unwrap().dump());

                discordWebhookSecret = val;
                discordConnectionCheck = true;
                if (data::getCBF()){
                    geode::Notification::create("You have CBF on! please disable it!", nullptr, 4)->show();
                }
                else if (data::getCBFAllowed()){
                    geode::Notification::create("CBF is allowed this match :D", nullptr, 4)->show();
                }

                data::checkConnectionComplete();
            });
        }
    );

    return Ok();
}

void data::checkConnectionComplete(std::string errMessage){
    //log::info("{} | {} - {}", discordConnectionCheck, sheetsConnectionCheck, errMessage);
    if (errMessage != "OK"){
        connecting = false;
        leaveMatch();

        if (m_target != nullptr)
            m_target->runAction(CCCallFuncO::create(m_target, m_callback, CCString::create(errMessage)));
    }

    if (discordConnectionCheck && sheetsConnectionCheck){
        isInMatch = true;
        connecting = false;
        if (m_target != nullptr)
            m_target->runAction(CCCallFuncO::create(m_target, m_callback, CCString::create("1")));
    }
}

void data::setConnectionCompleteCallback(SEL_CallFuncO callback, CCNode* target){
    m_callback = callback;
    m_target = target;
}

DiscordEmbed data::embedWithPlayerColor(){
    DiscordEmbed toReturn;

    auto RGBColor = GameManager::get()->colorForIdx(GameManager::get()->m_playerColor);

    toReturn.color = (RGBColor.r << 16) | (RGBColor.g << 8) | RGBColor.b;
    return toReturn;
}

arc::Future<Result<>> data::SendDiscordMessage(DiscordMessage message){
    web::WebRequest req = web::WebRequest();

    req.bodyJSON(matjson::Value(message));

    auto res = co_await req.post(discordWebhookLink + discordWebhookSecret);

    if (!res.ok()){
        co_return Err("Connection Failed!");
    }

    auto json = res.json();

    if (json.isOk()){
        co_return Err("Incorrect Secret!");
    }

    co_return Ok();
}

int data::getCombo(int levelID, int precent){
    int currentCombo = std::get<2>(lastLevelProgress);

    if (levelID == std::get<0>(lastLevelProgress) && precent == std::get<1>(lastLevelProgress)){
        currentCombo++;
    }
    else{
        currentCombo = 0;
    }

    std::get<0>(lastLevelProgress) = levelID;
    std::get<1>(lastLevelProgress) = precent;
    std::get<2>(lastLevelProgress) = currentCombo;

    return currentCombo;
}

arc::Future<Result<std::string>> data::refreshAccessToken(std::string clientId, std::string clientSecret, std::string refreshToken) {
    web::WebRequest req = web::WebRequest();

    req.param("client_id", clientId);
    req.param("client_secret", clientSecret);
    req.param("refresh_token", refreshToken);
    req.param("grant_type", "refresh_token");

    auto res = co_await req.post("https://oauth2.googleapis.com/token");

    auto jsonRes = res.json();
    if (jsonRes.isErr()) co_return Err("(refreshing) Invalid sheet data!");
    
    auto json = jsonRes.unwrap();

    auto sheetObjRes = json.as<sheetRefreshedToken>();
    if (sheetObjRes.isErr()) co_return Err("(refreshing) Invalid sheet obj data!");

    auto t = sheetObjRes.unwrap();

    if (t.access_token.empty()) co_return Err("Failed getting access token!");

    co_return Ok(t.access_token);
}

arc::Future<Result<>> data::writeToGoogleSheet(std::string spreadsheetId, std::string range, std::string value, std::string accessToken) {
    web::WebRequest req = web::WebRequest();

    sheetValues values;
    values.range = range;
    values.majorDimension = "ROWS";
    values.values = std::vector<std::vector<std::string>>{std::vector<std::string>{value}};

    req.header("Authorization", "Bearer " + accessToken);
    req.header("Content-Type", "application/json");
    req.bodyJSON(matjson::Value(values));

    req.param("valueInputOption", "USER_ENTERED");

    auto res = co_await req.put(fmt::format("https://sheets.googleapis.com/v4/spreadsheets/{}/values/{}", spreadsheetId, range));

    auto jsonRes = res.json();
    if (jsonRes.isErr()) co_return Err("(writing) Invalid sheet data!");
    
    auto json = jsonRes.unwrap();

    auto sheetObjRes = json.as<sheetWriteReport>();
    if (sheetObjRes.isErr()){
        log::info("{}", json.dump());
        co_return Err("(writing) Invalid sheet obj data!");
    }

    auto r = sheetObjRes.unwrap();
    
    if (r.spreadsheetId.empty()) co_return Err("failed writing to the sheet!");

    co_return Ok();
}

std::string data::columnNumberToLetter(int colNum) {
    colNum += 1;
    std::string colLetter;
    while (colNum > 0) {
        int rem = (colNum - 1) % 26;
        colLetter = static_cast<char>(rem + 'A') + colLetter;
        colNum = (colNum - 1) / 26;
    }
    return colLetter;
}

arc::Future<Result<>> data::SendSheetProgress(std::string message){
    auto res = co_await data::refreshAccessToken(sheetsClientID, sheetsClientSecret, sheetsRefreshToken);
    
    if (res.isErr())
        co_return Err("Invalid access token.");
    
    auto accToken = res.unwrap();

    auto res2 = co_await data::getCurrentMatchData(accToken);
    if (res2.isErr())
        co_return Err("Failed to get current sheet data.");
        
    auto currMatchPlayers = res2.unwrap();

    auto accID = std::to_string(GJAccountManager::get()->m_accountID);

    if (currMatchPlayers.contains(accID)){
        std::string rangeLetter = currMatchPlayers[accID].first;
        int rangeNum = currMatchPlayers[accID].second.size() + 2;

        auto res3 = co_await data::writeToGoogleSheet(matchSheetID, fmt::format("{}!{}{}", matchSheetName, rangeLetter, rangeNum), message, accToken);

        if (res3.isOk()){
            co_return Ok();
        }   
        else{
            co_return Err(res3.unwrapErr());
        }
    }

    co_return Err("Not part of match!");
}

bool data::getCBF(){
    if (getCBFAllowed()) return false;

    if (auto cbf = Loader::get()->getLoadedMod("syzzi.click_between_frames")){
        if (!cbf->isOrWillBeEnabled())
            return false;

        if (cbf->getSettingValue<bool>("soft-toggle"))
            return false;

        if (cbf->getSettingValue<bool>("click-on-steps"))
            return false;

        return true;
    }

    return false;
}

bool data::getCBFAllowed(){
    return CBFAllowed;
}

void data::disable2point1Percent(GJGameLevel* level){
    auto pt = Loader::get()->getLoadedMod("zsa.percentage-toggle");

    if (pt == nullptr) return;

    if (pt->getSettingValue<bool>("force-enable")){
        pt->setSettingValue("force-enable", false);
        Notification::create("Disabled \"force-enable 2.1%\" as its not allowed in GDWT")->show();
        return;
    }

    // (percentage toggle level key code)
    std::string levelKey;
	if(level->m_levelID == 0){
		levelKey = fmt::format("{}_{}", level->m_levelName, level->m_levelRev);
	}
	else{
		levelKey = std::to_string(level->m_levelID);
	}

    auto toggleData = pt->getSavedValue<std::vector<ZSAToggleSaveData>>("toggle-save-data");
	
    for (int i = 0; i < toggleData.size(); i++)
    {
        if (toggleData[i].key == levelKey && toggleData[i].toggled){
            toggleData[i].toggled = false;
            pt->setSavedValue("toggle-save-data", toggleData);
            Notification::create("Disabled 2.1% as its not allowed in GDWT")->show();
            return;
        }
    }
}

void data::sendError(std::string err){
    geode::queueInMainThread([err](){
        Notification::create(err, nullptr)->show();
    });
}
