/* bzflag
 * Copyright (c) 1993 - 2007 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* interface header */
#include "ServerMenu.h"

/* common implementation headers */
#include "FontManager.h"
#include "TextUtils.h"
#include "bzglob.h"
#include "AnsiCodes.h"
#include "BundleMgr.h"
#include "Bundle.h"

/* local implementation headers */
#include "FontSizer.h"
#include "MainMenu.h"
#include "HUDDialogStack.h"
#include "playing.h"
#include "HUDui.h"
#include "HUDNavigationQueue.h"

const int ServerMenu::NumReadouts = 22;
const int ServerMenu::NumItems = 10;


bool ServerMenuDefaultKey::keyPress(const BzfKeyEvent& key)
{
  if (key.ascii == 0) switch (key.button) {
    case BzfKeyEvent::Up:
      if (HUDui::getFocus()) {
	if (!menu->getFind())
	  menu->setSelected(menu->getSelected() - 1);
	else
	  menu->setFind(false);
      }
      return true;

    case BzfKeyEvent::Down:
      if (HUDui::getFocus()) {
	if (!menu->getFind())
	  menu->setSelected(menu->getSelected() + 1);
	else
	  menu->setFind(false);
      }
      return true;

    case BzfKeyEvent::PageUp:
      if (HUDui::getFocus()) {
	if (!menu->getFind())
	  menu->setSelected(menu->getSelected() - ServerMenu::NumItems);
	else
	  menu->setFind(false);
      }
      return true;

    case BzfKeyEvent::PageDown:
      if (HUDui::getFocus()) {
	if (!menu->getFind())
	  menu->setSelected(menu->getSelected() + ServerMenu::NumItems);
	else
	  menu->setFind(false);
      }
      return true;

  }

  else if (key.ascii == '\t') {
    if (HUDui::getFocus()) {
      menu->setSelected(menu->getSelected() + 1);
    }
    return true;
  }

  else if (key.ascii == '/') {
    if (HUDui::getFocus() && !menu->getFind()) {
      menu->setFind(true);
      return true;
    }
  }

  else if (key.ascii == 'f') {
    if (HUDui::getFocus() && !menu->getFind()) {
      menu->toggleFavView();
      return true;
    }
  }

  else if (key.ascii == '+') {
    if (HUDui::getFocus() && !menu->getFind()) {
      menu->setFav(true);
      return true;
    }
  }

  else if (key.ascii == '-') {
    if (HUDui::getFocus() && !menu->getFind()) {
      menu->setFav(false);
      return true;
    }
  }

  else if (key.ascii == 27) {
    if (HUDui::getFocus()) {
      // escape drops out of find mode
      // note that this is handled by MenuDefaultKey if we're not in find mode
      if (menu->getFind()) {
	menu->setFind(false);
	return true;
      }
    }
  }

  return MenuDefaultKey::keyPress(key);
}

bool ServerMenuDefaultKey::keyRelease(const BzfKeyEvent& key)
{
  switch (key.button) {
    case BzfKeyEvent::Up:
    case BzfKeyEvent::Down:
    case BzfKeyEvent::PageUp:
    case BzfKeyEvent::PageDown:
      return true;
  }
  switch (key.ascii) {
    case 27:	// escape
    case 13:	// return
      return true;
  }
  return false;
}

ServerMenu::ServerMenu() : defaultKey(this),
			   selectedIndex(-1),
			   serversFound(0),
			   findMode(false),
			   filter("*"),
			   favView(false),
			   newfilter(false)
{
  // add controls
  readouts.push_back(addLabel("Servers", ""));
  readouts.push_back(addLabel("Players", ""));
  readouts.push_back(addLabel("Rogue", ""));
  readouts.push_back(addLabel("Red", ""));
  readouts.push_back(addLabel("Green", ""));
  readouts.push_back(addLabel("Blue", ""));
  readouts.push_back(addLabel("Purple", ""));
  readouts.push_back(addLabel("Observers", ""));
  readouts.push_back(addLabel("", ""));		// max shots
  readouts.push_back(addLabel("", ""));		// capture-the-flag/free-style/rabbit chase
  readouts.push_back(addLabel("", ""));		// super-flags
  readouts.push_back(addLabel("", ""));		// antidote-flag
  readouts.push_back(addLabel("", ""));		// shaking time
  readouts.push_back(addLabel("", ""));		// shaking wins
  readouts.push_back(addLabel("", ""));		// jumping
  readouts.push_back(addLabel("", ""));		// ricochet
  readouts.push_back(addLabel("", ""));		// inertia
  readouts.push_back(addLabel("", ""));		// time limit
  readouts.push_back(addLabel("", ""));		// max team score
  readouts.push_back(addLabel("", ""));		// max player score
  readouts.push_back(addLabel("", ""));		// cached status
  readouts.push_back(addLabel("", ""));		// cached age
  status = addLabel("", "", true);	        // search status
  pageLabel = addLabel("", "");		        // page readout

  // add server list items
  for (int i = 0; i < NumItems; ++i)
    items.push_back(addLabel("", "", true));

  // find server
  search = new HUDuiTypeIn;
  search->setFontFace(MainMenu::getFontFace());
  search->setMaxLength(30);
  addControl(search);

  // short key help
  help = new HUDuiLabel;
  help->setFontFace(MainMenu::getFontFace());
  if (serverList.size() == 0) {
    help->setString("");
  } else {
    help->setString("Press  +/- add/remove favorites   f - toggle view");
  }
  addControl(help, false);

  setFind(false);

  // set initial focus
  initNavigation();
  if (serverList.size() == 0)
    getNav().set(status);
  
  setSelected(0);
}


HUDuiLabel* ServerMenu::addLabel(const char* msg, const char* _label, bool navigable)
{
  HUDuiLabel* label = new HUDuiLabel;
  label->setFontFace(MainMenu::getFontFace());
  label->setString(msg);
  label->setLabel(_label);
  addControl(label, navigable); // non-navigable by default
  return label;
}

void ServerMenu::setFind(bool mode)
{
  std::string oldfilter = filter;
  if (mode) {
    search->setLabel("Find Servers:");
    getNav().set(search);
  } else {
    if (search->getString() == "" || search->getString() == "*") {
      if (serverList.size() == 0) {
	search->setLabel("");
	help->setString("");
      } else {
	search->setLabel("Press '/' to search");
	help->setString("Press  +/- add/remove favorites   f - toggle view");
      }
      search->setString("");
      filter = "*";
    } else {
      if (search->getString().find("*") == std::string::npos
	&& search->getString().find("?") == std::string::npos)
	search->setString("*" + search->getString() + "*");
      search->setLabel("Using filter:");
      // filter is set in lower case
      filter = TextUtils::tolower(search->getString());
    }
    newfilter = true;
    // select the first item in the list
    updateStatus();
    setSelected(0);
  }
  findMode = mode;

  newfilter = (filter != oldfilter);
  updateStatus();
}

bool ServerMenu::getFind() const
{
  return findMode;
}

void ServerMenu::toggleFavView()
{
  favView = !favView;
  newfilter = true;
  updateStatus();
}

void ServerMenu::setFav(bool fav)
{
  if (serverList.size() == 0 || selectedIndex < 0) {
    return;
  }

  const ServerItem& item = serverList.getServers()[selectedIndex];
  std::string addrname = item.getAddrName();
  ServerListCache *cache = ServerListCache::get();
  ServerListCache::SRV_STR_MAP::iterator i = cache->find(addrname);
  if (i!= cache->end()) {
    i->second.favorite = fav;
  } else {
    // FIXME  should not ever come here, but what to do?
  }
  realServerList.markFav(addrname, fav);
  serverList.markFav(addrname, fav);

  setSelected(getSelected()+1, true);
}

int ServerMenu::getSelected() const
{
  return selectedIndex;
}

void ServerMenu::setSelected(int index, bool forcerefresh)
{
  // clamp index
  if (index < 0)
    index = (int)serverList.size() - 1;
  else if (index != 0 && index >= (int)serverList.size())
    index = 0;

  // ignore if no change
  if (!forcerefresh && selectedIndex == index)
    return;

  // update selected index and get old and new page numbers
  const int oldPage = (selectedIndex < 0) ? -1 : (selectedIndex / NumItems);
  selectedIndex = index;
  const int newPage = (selectedIndex / NumItems);

  // if page changed then load items for this page
  if (oldPage != newPage || forcerefresh) {
    // fill items
    const int base = newPage * NumItems;
    for (int i = 0; i < NumItems; ++i) {
      HUDuiLabel* label = items[i];
      if (base + i < (int)serverList.size()) {
	const ServerItem &server = serverList.getServers()[base + i];
	const short gameType = server.ping.gameType;
	const short gameOptions = server.ping.gameOptions;
	std::string fullLabel;
	if (BZDB.isTrue("listIcons")) {
	  // game mode
	  if ((server.ping.observerMax == 16) &&
	      (server.ping.maxPlayers == 200)) {
	    fullLabel += ANSI_STR_FG_CYAN "*  "; // replay
	  } else if (gameType == eClassicCTF) {
	    fullLabel += ANSI_STR_FG_RED "*  "; // ctf
	  } else if (gameType == eRabbitChase) {
	    fullLabel += ANSI_STR_FG_WHITE "*  "; // white rabbit
	  } else {
	    fullLabel += ANSI_STR_FG_YELLOW "*  "; // free-for-all
	  }
	  // jumping?
	  if (gameOptions & JumpingGameStyle) {
	    fullLabel += ANSI_STR_BRIGHT ANSI_STR_FG_MAGENTA "J ";
	  } else {
	    fullLabel += ANSI_STR_DIM ANSI_STR_FG_WHITE "J ";
	  }
	  // superflags ?
	  if (gameOptions & SuperFlagGameStyle) {
	    fullLabel += ANSI_STR_BRIGHT ANSI_STR_FG_BLUE "F ";
	  } else {
	    fullLabel += ANSI_STR_DIM ANSI_STR_FG_WHITE "F ";
	  }
	  // ricochet?
	  if (gameOptions & RicochetGameStyle) {
	    fullLabel += ANSI_STR_BRIGHT ANSI_STR_FG_GREEN "R";
	  } else {
	    fullLabel += ANSI_STR_DIM ANSI_STR_FG_WHITE "R";
	  }
	  fullLabel += ANSI_STR_RESET "   ";

	  // colorize server descriptions by shot counts
	  const int maxShots = server.ping.maxShots;
	  if (maxShots <= 0) {
	    label->setColor(0.4f, 0.0f, 0.6f); // purple
	  }
	  else if (maxShots == 1) {
	    label->setColor(0.25f, 0.25f, 1.0f); // blue
	  }
	  else if (maxShots == 2) {
	    label->setColor(0.25f, 1.0f, 0.25f); // green
	  }
	  else if (maxShots == 3) {
	    label->setColor(1.0f, 1.0f, 0.25f); // yellow
	  }
	  else {
	    // graded orange/red
	    const float shotScale =
	      std::min(1.0f, log10f((float)(maxShots - 3)));
	    const float rf = 1.0f;
	    const float gf = 0.4f * (1.0f - shotScale);
	    const float bf = 0.25f * gf;
	    label->setColor(rf, gf, bf);
	  }
	}
	else {
	  // colorize servers: many shots->red, jumping->green, CTF->blue
	  const float rf = std::min(1.0f, logf(server.ping.maxShots) / logf(20.0f));
	  const float gf = gameOptions & JumpingGameStyle ? 1.0f : 0.0f;
	  const float bf = (gameType == eClassicCTF) ? 1.0f : 0.0f;
	  label->setColor(0.5f + rf * 0.5f, 0.5f + gf * 0.5f, 0.5f + bf * 0.5f);
	}

	std::string addr = stripAnsiCodes(server.description.c_str());
	std::string desc;
	std::string::size_type pos = addr.find_first_of(';');
	if (pos != std::string::npos) {
	  desc = addr.substr(pos > 0 ? pos+1 : pos);
	  addr.resize(pos);
	}
	if (server.favorite)
	  fullLabel += ANSI_STR_FG_ORANGE;
	else
	  fullLabel += ANSI_STR_FG_WHITE;
	fullLabel += addr;
	fullLabel += ANSI_STR_RESET " ";
	fullLabel += desc;
	label->setString(fullLabel);
	label->setDarker(server.cached);
      }
      else {
	label->setString("");
      }
    }

    // change page label
    if ((int)serverList.size() > NumItems) {
      char msg[50];
      std::vector<std::string> args;
      sprintf(msg, "%d", newPage + 1);
      args.push_back(msg);
      sprintf(msg, "%ld", (long int)(serverList.size() + NumItems - 1) / NumItems);
      args.push_back(msg);
      pageLabel->setString("Page {1} of {2}", &args);
    }
  }

  // set focus to selected item
  if (serverList.size() > 0) {
    const int indexOnPage = selectedIndex % NumItems;
    getNav().set(items[indexOnPage]);
  }

  // update readouts
  pick();
}

void ServerMenu::pick()
{
  if (serverList.size() == 0 || selectedIndex < 0)
    return;

  // get server info
  const ServerItem& item = serverList.getServers()[selectedIndex];
  const PingPacket& ping = item.ping;

  // update server readouts
  char buf[60];
  std::vector<HUDuiLabel*>& listHUD = readouts;

  const uint8_t maxes [] = { ping.maxPlayers, ping.rogueMax, ping.redMax, ping.greenMax,
      ping.blueMax, ping.purpleMax, ping.observerMax };

  // if this is a cached item set the player counts to "?/max count"
  if (item.cached && item.getPlayerCount() == 0) {
    for (int i = 1; i <=7; i ++){
      sprintf(buf, "?/%d", maxes[i-1]);
      (listHUD[i])->setLabel(buf);
    }
  } else {  // not an old item, set players #s to info we have
    sprintf(buf, "%d/%d", ping.rogueCount + ping.redCount + ping.greenCount +
	ping.blueCount + ping.purpleCount, ping.maxPlayers);
    (listHUD[1])->setLabel(buf);

    if (ping.rogueMax == 0)
      buf[0]=0;
    else if (ping.rogueMax >= ping.maxPlayers)
      sprintf(buf, "%d", ping.rogueCount);
    else
      sprintf(buf, "%d/%d", ping.rogueCount, ping.rogueMax);
    (listHUD[2])->setLabel(buf);

    if (ping.redMax == 0)
      buf[0]=0;
    else if (ping.redMax >= ping.maxPlayers)
      sprintf(buf, "%d", ping.redCount);
    else
      sprintf(buf, "%d/%d", ping.redCount, ping.redMax);
    (listHUD[3])->setLabel(buf);

    if (ping.greenMax == 0)
      buf[0]=0;
    else if (ping.greenMax >= ping.maxPlayers)
      sprintf(buf, "%d", ping.greenCount);
    else
      sprintf(buf, "%d/%d", ping.greenCount, ping.greenMax);
    (listHUD[4])->setLabel(buf);

    if (ping.blueMax == 0)
      buf[0]=0;
    else if (ping.blueMax >= ping.maxPlayers)
      sprintf(buf, "%d", ping.blueCount);
    else
      sprintf(buf, "%d/%d", ping.blueCount, ping.blueMax);
    (listHUD[5])->setLabel(buf);

    if (ping.purpleMax == 0)
      buf[0]=0;
    else if (ping.purpleMax >= ping.maxPlayers)
      sprintf(buf, "%d", ping.purpleCount);
    else
      sprintf(buf, "%d/%d", ping.purpleCount, ping.purpleMax);
    (listHUD[6])->setLabel(buf);

    if (ping.observerMax == 0)
      buf[0]=0;
    else if (ping.observerMax >= ping.maxPlayers)
      sprintf(buf, "%d", ping.observerCount);
    else
      sprintf(buf, "%d/%d", ping.observerCount, ping.observerMax);
    (listHUD[7])->setLabel(buf);
  }

  std::vector<std::string> args;
  sprintf(buf, "%d", ping.maxShots);
  args.push_back(buf);

  if (ping.maxShots == 1)
    (listHUD[8])->setString("{1} Shot", &args );
  else
    (listHUD[8])->setString("{1} Shots", &args );

  if (ping.gameType == eClassicCTF)
    (listHUD[9])->setString("Classic Capture-the-Flag");
  else if (ping.gameType == eRabbitChase)
    (listHUD[9])->setString("Rabbit Chase");
  else if (ping.gameType == eOpenFFA)
	  (listHUD[9])->setString("Open (Teamless) Free-For-All");
  else
    (listHUD[9])->setString("Team Free-For-All");

  if (ping.gameOptions & SuperFlagGameStyle)
    (listHUD[10])->setString("Super Flags");
  else
    (listHUD[10])->setString("");

  if (ping.gameOptions & AntidoteGameStyle)
    (listHUD[11])->setString("Antidote Flags");
  else
    (listHUD[11])->setString("");

  if ((ping.gameOptions & ShakableGameStyle) && ping.shakeTimeout != 0) {
    std::vector<std::string> dropArgs;
    sprintf(buf, "%.1f", 0.1f * float(ping.shakeTimeout));
    dropArgs.push_back(buf);
    if (ping.shakeWins == 1)
      (listHUD[12])->setString("{1} sec To Drop Bad Flag",
					    &dropArgs);
    else
      (listHUD[12])->setString("{1} secs To Drop Bad Flag",
					    &dropArgs);
  }
  else
    (listHUD[13])->setString("");

  if ((ping.gameOptions & ShakableGameStyle) && ping.shakeWins != 0) {
    std::vector<std::string> dropArgs;
    sprintf(buf, "%d", ping.shakeWins);
    dropArgs.push_back(buf);
    dropArgs.push_back(ping.shakeWins == 1 ? "" : "s");
    if (ping.shakeWins == 1)
      (listHUD[12])->setString("{1} Win Drops Bad Flag",
					    &dropArgs);
    else
      (listHUD[12])->setString("{1} Wins Drops Bad Flag",
					    &dropArgs);
  }
  else
    (listHUD[13])->setString("");

  if (ping.gameOptions & JumpingGameStyle)
    (listHUD[14])->setString("Jumping");
  else
    (listHUD[14])->setString("");

  if (ping.gameOptions & RicochetGameStyle)
    (listHUD[15])->setString("Ricochet");
  else
    (listHUD[15])->setString("");

  if (ping.gameOptions & HandicapGameStyle)
    (listHUD[16])->setString("Handicap");
  else
    (listHUD[16])->setString("");

  if (ping.maxTime != 0) {
    std::vector<std::string> pingArgs;
    if (ping.maxTime >= 3600)
      sprintf(buf, "%d:%02d:%02d", ping.maxTime / 3600, (ping.maxTime / 60) % 60, ping.maxTime % 60);
    else if (ping.maxTime >= 60)
      sprintf(buf, "%d:%02d", ping.maxTime / 60, ping.maxTime % 60);
    else
      sprintf(buf, "0:%02d", ping.maxTime);
    pingArgs.push_back(buf);
    (listHUD[17])->setString("Time limit: {1}", &pingArgs);
  } else {
    (listHUD[17])->setString("");
  }

  if (ping.maxTeamScore != 0) {
    std::vector<std::string> scoreArgs;
    sprintf(buf, "%d", ping.maxTeamScore);
    scoreArgs.push_back(buf);
    (listHUD[18])->setString("Max team score: {1}", &scoreArgs);
  }
  else
    (listHUD[18])->setString("");


  if (ping.maxPlayerScore != 0) {
    std::vector<std::string> scoreArgs;
    sprintf(buf, "%d", ping.maxPlayerScore);
    scoreArgs.push_back(buf);
    (listHUD[19])->setString("Max player score: {1}", &scoreArgs);
  } else {
    (listHUD[19])->setString("");
  }

  if (item.cached){
    (listHUD[20])->setString("Cached");
    (listHUD[21])->setString(item.getAgeString());
  }
  else {
    (listHUD[20])->setString("");
    (listHUD[21])->setString("");
  }
}

void			ServerMenu::show()
{
  // clear server readouts
  std::vector<HUDuiLabel*>& listHUD = readouts;
  (listHUD[1])->setLabel("");
  (listHUD[2])->setLabel("");
  (listHUD[3])->setLabel("");
  (listHUD[4])->setLabel("");
  (listHUD[5])->setLabel("");
  (listHUD[6])->setLabel("");
  (listHUD[7])->setLabel("");
  (listHUD[8])->setString("");
  (listHUD[9])->setString("");
  (listHUD[10])->setString("");
  (listHUD[11])->setString("");
  (listHUD[12])->setString("");
  (listHUD[13])->setString("");
  (listHUD[14])->setString("");
  (listHUD[15])->setString("");
  (listHUD[16])->setString("");
  (listHUD[17])->setString("");
  (listHUD[18])->setString("");
  (listHUD[19])->setString("");
  (listHUD[20])->setString("");
  (listHUD[21])->setString("");

  // add cache items w/o re-caching them
  serversFound = 0;
  int numItemsAdded;
  numItemsAdded = realServerList.updateFromCache();

  // update the status
  updateStatus();

  // focus to no-server
  getNav().set(status);

  // *** NOTE *** start ping here
  // listen for echos
  addPlayingCallback(&playingCB, this);
  realServerList.startServerPings(getStartupInfo());

}

void			ServerMenu::execute()
{
  HUDuiControl* _focus = getNav().get();
  if (_focus == search) {
    setFind(false);
    return;
  }

  if (selectedIndex < 0 || selectedIndex >= (int)serverList.size())
    return;

  // update startup info
  StartupInfo* info = getStartupInfo();
  strcpy(info->serverName, serverList.getServers()[selectedIndex].name.c_str());
  info->serverPort = ntohs((unsigned short)
				serverList.getServers()[selectedIndex].ping.serverId.port);

  // all done
  HUDDialogStack::get()->pop();
}

void			ServerMenu::dismiss()
{
  // no more callbacks
  removePlayingCallback(&playingCB, this);
  // save any new token we got
  // FIXME myTank.token = serverList.token;
}

void			ServerMenu::resize(int _width, int _height)
{
  // remember size
  HUDDialog::resize(_width, _height);
  FontSizer fs = FontSizer(_width, _height);

  const int menuFontFace = MainMenu::getFontFace();
  FontManager &fm = FontManager::instance();

  // reposition title
  float x, y;
  {
    HUDuiLabel* title = readouts[0];
    const int fontFace = title->getFontFace();

    // use a big font for title, smaller font for the rest
    fs.setMin(0, (int)(1.0 / BZDB.eval("headerFontSize") / 2.0));
    const float titleFontSize = fs.getFontSize(fontFace, "headerFontSize");
    
    title->setFontSize(titleFontSize);
    const float titleWidth = fm.getStringWidth(fontFace, titleFontSize, title->getString().c_str());
    const float titleHeight = fm.getStringHeight(fontFace, titleFontSize);
    x = 0.5f * ((float)_width - titleWidth);
    y = (float)_height - titleHeight;
    title->setPosition(x, y);
  }

  // reposition server readouts
  int i;
  const float y0 = y;
  fs.setMin(10, 10);
  float fontSize = fs.getFontSize(menuFontFace, "alertFontSize");
  float fontHeight = fm.getStringHeight(menuFontFace, fontSize);
  for (i = 1; i < NumReadouts - 2; i++) {
    if (i % 7 == 1) {
      x = (0.125f + 0.25f * (float)((i - 1) / 7)) * (float)_width;
      y = y0;
    }

    HUDuiLabel* label = readouts[i];
    label->setFontSize(fontSize);
    y -= 1.0f * fontHeight;
    label->setPosition(x, y);
  }

  y = (readouts[7])->getY(); //reset bottom to last team label

  // reposition search status readout
  {
    fontSize = fs.getFontSize(menuFontFace, "menuFontSize");
    float fontHt = fm.getStringHeight(menuFontFace, fontSize);
    status->setFontSize(fontSize);
    const float statusWidth = fm.getStringWidth(status->getFontFace(), fontSize, status->getString().c_str());
    x = 0.5f * ((float)_width - statusWidth);
    y -= 1.2f * fontHt;
    status->setPosition(x, y);
  }

  // reposition find server input
  {
    fontSize = fs.getFontSize(menuFontFace, "menuFontSize");
    float fontHt = fm.getStringHeight(menuFontFace, fontSize);
    search->setFontSize(fontSize);
    const float searchWidth = fm.getStringWidth(search->getFontFace(), fontSize, search->getString().c_str());
    x = 0.5f * ((float)_width - searchWidth);
    search->setPosition(x, fontHt * 2 /* near bottom of screen */);
  }

  // reposition key help
  {
    Bundle *bdl = BundleMgr::getCurrentBundle(); //all to make sure it's in the right place
    std::string realtext = bdl->getLocalString("Press  +/- add/remove favorites   f - toggle view");
    std::cout << help->getString() << std::endl;
    fontSize = fs.getFontSize(menuFontFace, "infoFontSize");
    float fontHt = fm.getStringHeight(menuFontFace, fontSize);
    help->setFontSize(fontSize);
    const float searchWidth = fm.getStringWidth(help->getFontFace(), fontSize, realtext.c_str()); //otherwise, this will take the length of an empty string
    x = 0.5f * ((float)_width - searchWidth);
    help->setPosition(x, (fontHt / 2) /* near bottom of screen */); //FIXME still broken
  }

  // position page readout and server item list
  fs.setMin(40,10);
  fontSize = fs.getFontSize(menuFontFace, "alertFontSize");
  fontHeight = fm.getStringHeight(menuFontFace, fontSize);
  x = 0.125f * (float)_width;
  const bool useIcons = BZDB.isTrue("listIcons");
  
  HUDuiLabel* pagelabel = readouts[NumReadouts - 1];
  pagelabel->setFontSize(fontSize);
  y -= 1.0f * fontHeight;
  pagelabel->setPosition(x, y);
    
  for (i = 0; i < NumItems; ++i) {
    HUDuiLabel* label = items[i];
    label->setFontSize(fontSize);
    y -= 1.0f * fontHeight;
    if (useIcons) {
      const float offset = fm.getStringWidth(status->getFontFace(), fontSize, "*  J F R   ");
      label->setPosition(x - offset, y);
    } else {
      label->setPosition(x, y);
    }
  }
}

void			ServerMenu::setStatus(const char* msg, const std::vector<std::string> *parms)
{
  status->setString(msg, parms);
  FontManager &fm = FontManager::instance();
  const float statusWidth = fm.getStringWidth(status->getFontFace(), status->getFontSize(), status->getString().c_str());
  status->setPosition(0.5f * ((float)width - statusWidth), status->getY());
}

void			ServerMenu::updateStatus() {
  // nothing here to see
  if (!realServerList.serverFound()) {
    setStatus("Searching");
    return;
  }

  // don't run unnecessarily
  if (realServersFound == realServerList.size() && !newfilter)
    return;

  // do filtering
  serverList.clear();
  for (unsigned int i = 0; i < realServerList.size(); ++i) {
    const ServerItem &item = realServerList.getServers()[i];
    // filter is already lower case.  do case insensitive matching.
    if ((glob_match(filter, TextUtils::tolower(item.description)) ||
	 glob_match(filter, TextUtils::tolower(item.name))) &&
	(!favView || item.favorite)
       ) {
      serverList.addToList(item);
    }
  }
  newfilter = false;

  // update the status label
  std::vector<std::string> args;
  char buffer [80];
  sprintf(buffer, "%d", (unsigned int)serverList.size());
  args.push_back(buffer);
  sprintf(buffer, "%d", (unsigned int)realServerList.size());
  args.push_back(buffer);
  if (favView)
    setStatus("Favorite servers: {1}/{2}", &args);
  else
    setStatus("Servers found: {1}/{2}", &args);
  pageLabel->setString("");
  selectedIndex = -1;
  setSelected(getSelected());

  serversFound = (unsigned int)serverList.size();
  realServersFound = (unsigned int)realServerList.size();
}


void			ServerMenu::playingCB(void* _self)
{
  ((ServerMenu*)_self)->realServerList.checkEchos(getStartupInfo());

  ((ServerMenu*)_self)->updateStatus();
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8
