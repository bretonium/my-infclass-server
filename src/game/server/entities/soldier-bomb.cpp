/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <game/server/gamecontext.h>
#include <engine/shared/config.h>
#include "soldier-bomb.h"
#include <cmath>

CSoldierBomb::CSoldierBomb(CGameWorld *pGameWorld, vec2 Pos, int Owner)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_SOLDIER_BOMB)
{
	m_Pos = Pos;
	GameWorld()->InsertEntity(this);
	m_DetectionRadius = 60.0f;
	m_StartTick = Server()->Tick();
	m_Owner = Owner;

	m_nbBomb = g_Config.m_InfSoldierBombs;
	charged_bomb = g_Config.m_InfSoldierBombs;

	m_IDBomb.set_size(g_Config.m_InfSoldierBombs);
	for(int i=0; i<m_IDBomb.size(); i++)
	{
		m_IDBomb[i] = Server()->SnapNewID();
	}
	for(int i=0; i<24; i++)
	{
		m_IDs[i] = Server()->SnapNewID();
	}
}

CSoldierBomb::~CSoldierBomb()
{
	for(int i=0; i<m_IDBomb.size(); i++)
		Server()->SnapFreeID(m_IDBomb[i]);
	for(int i=0; i<24; i++)
	{
		Server()->SnapFreeID(m_IDs[i]);
	}
}

void CSoldierBomb::Reset()
{
	GameServer()->m_World.DestroyEntity(this);
}

void CSoldierBomb::Explode()
{
	CCharacter *OwnerChar = GameServer()->GetPlayerChar(m_Owner);
	if(!OwnerChar)
		return;
		
	vec2 dir = normalize(OwnerChar->m_Pos - m_Pos);
	
	
	GameServer()->CreateSound(m_Pos, SOUND_GRENADE_EXPLODE);
	GameServer()->CreateExplosion(m_Pos, m_Owner, WEAPON_HAMMER, false, TAKEDAMAGEMODE_NOINFECTION);
	if (charged_bomb <= m_nbBomb) {
		/*
		 * Charged bomb makes a big explosion.
		 *
		 * there are m_nbBomb bombs. Firstly, bomb #m_nbBomb explodes. Then
		 * #m_nbBomb-1. Then #m_nbBomb-2. If there are 3 bombs, bomb #3 explodes
		 * first, bomb #2 second, bomb #1 last.
		 * charged_bomb points to the last charged bomb. So, if charged_bomb == 1,
		 * it means that bombs #m_mbBomb, ..., #2, #1 are charged.
		 */
		for(int i=0; i<6; i++)
		{
			float angle = static_cast<float>(i)*2.0*pi/6.0;
			vec2 expPos = m_Pos + vec2(90.0*cos(angle), 90.0*sin(angle));
			GameServer()->CreateExplosion(expPos, m_Owner, WEAPON_HAMMER, false, TAKEDAMAGEMODE_NOINFECTION);
		}
		for(int i=0; i<12; i++)
		{
			float angle = static_cast<float>(i)*2.0*pi/12.0;
			vec2 expPos = vec2(180.0*cos(angle), 180.0*sin(angle));
			if(dot(expPos, dir) <= 0)
			{
				GameServer()->CreateExplosion(m_Pos + expPos, m_Owner, WEAPON_HAMMER, false, TAKEDAMAGEMODE_NOINFECTION);
			}
		}
	}
	
	m_nbBomb--;
	
	if(m_nbBomb == 0)
	{
		OwnerChar->m_aSoldier.m_CurrentBomb = NULL;
		GameServer()->m_World.DestroyEntity(this);
	}
}

void CSoldierBomb::ChargeBomb(float time)
{
	if (charged_bomb > 1) {
		// time is multiplied by N, bombs will get charged every 1/N sec
		if (std::floor(time * 1.4) >
				g_Config.m_InfSoldierBombs - charged_bomb) {
			charged_bomb--;
			GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO);
		}
	}
}

void CSoldierBomb::Snap(int SnappingClient)
{
	float time = (Server()->Tick()-m_StartTick)/(float)Server()->TickSpeed();
	float angle = fmodf(time*pi/2, 2.0f*pi);
	ChargeBomb(time);

	for(int i=0; i<m_nbBomb; i++)
	{
		if(NetworkClipped(SnappingClient))
			return;
		
		float shiftedAngle = angle + 2.0*pi*static_cast<float>(i)/static_cast<float>(m_IDBomb.size());
		
		CNetObj_Projectile *pProj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, m_IDBomb[i], sizeof(CNetObj_Projectile)));
		pProj->m_X = (int)(m_Pos.x + m_DetectionRadius*cos(shiftedAngle));
		pProj->m_Y = (int)(m_Pos.y + m_DetectionRadius*sin(shiftedAngle));
		pProj->m_VelX = (int)(0.0f);
		pProj->m_VelY = (int)(0.0f);
		pProj->m_StartTick = Server()->Tick();
		pProj->m_Type = WEAPON_GRENADE;
	}

	if(GameServer()->GetPlayerChar(m_Owner)->m_PositionLocked)
	{
		float Radius = m_DetectionRadius + 20;
		
		int NumSide = 12;
		
		float AngleStep = 2.0f * pi / NumSide;
		
		for(int i=0; i<NumSide; i++)
		{
			vec2 PartPosStart = m_Pos + vec2(Radius * cos(AngleStep*i), Radius * sin(AngleStep*i));
			vec2 PartPosEnd = m_Pos + vec2(Radius * cos(AngleStep*(i+1)), Radius * sin(AngleStep*(i+1)));
			
			CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, m_IDs[i], sizeof(CNetObj_Laser)));
			if(!pObj)
				return;

			pObj->m_X = (int)PartPosStart.x;
			pObj->m_Y = (int)PartPosStart.y;
			pObj->m_FromX = (int)PartPosEnd.x;
			pObj->m_FromY = (int)PartPosEnd.y;
			pObj->m_StartTick = Server()->Tick();
		}
	}
}

void CSoldierBomb::TickPaused()
{
	++m_StartTick;
}

bool CSoldierBomb::AddBomb()
{
	if(m_nbBomb < m_IDBomb.size())
	{
		m_nbBomb++;
		return true;
	}
	else return false;
}
