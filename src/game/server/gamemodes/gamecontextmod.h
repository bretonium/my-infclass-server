#ifndef GAME_SERVER_GAMECONTEXT_MOD_H
#define GAME_SERVER_GAMECONTEXT_MOD_H

#include <game/server/gamecontext.h>

class CGameContextMod : public CGameContext
{
public:
	CGameContextMod();

	void OnTick() override;
};

#endif
