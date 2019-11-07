#include "gamecontextmod.h"

CGameContextMod::CGameContextMod()
	: CGameContext()
{
}

IGameServer *CreateGameServer() { return new CGameContextMod; }
