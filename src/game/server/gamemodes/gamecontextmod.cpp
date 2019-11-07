#include "gamecontextmod.h"

#include <engine/shared/config.h>
#include <engine/server/roundstatistics.h>

CGameContextMod::CGameContextMod()
	: CGameContext()
{
}

void CGameContextMod::OnTick()
{
	for(int i=0; i<MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
		{
			//Show top10
			if(!Server()->GetClientMemory(i, CLIENTMEMORY_TOP10))
			{
				if(!g_Config.m_SvMotd[0] || Server()->GetClientMemory(i, CLIENTMEMORY_ROUNDSTART_OR_MAPCHANGE))
				{
#ifdef CONF_SQL
					Server()->ShowChallenge(i);
#endif
					Server()->SetClientMemory(i, CLIENTMEMORY_TOP10, true);
				}
			}
		}
	}

	//Target to kill
	if(m_TargetToKill >= 0 && (!m_apPlayers[m_TargetToKill] || !m_apPlayers[m_TargetToKill]->GetCharacter()))
	{
		m_TargetToKill = -1;
	}

	int LastTarget = -1;
	// Zombie is in InfecZone too long -> change target
	if(m_TargetToKill >= 0 && m_apPlayers[m_TargetToKill] && m_apPlayers[m_TargetToKill]->GetCharacter() && (m_apPlayers[m_TargetToKill]->GetCharacter()->GetInfZoneTick()*Server()->TickSpeed()) > 1000*g_Config.m_InfNinjaTargetAfkTime) 
	{
		LastTarget = m_TargetToKill;
		m_TargetToKill = -1;
	}

	if(m_HeroGiftCooldown > 0)
		m_HeroGiftCooldown--;

	if(m_TargetToKillCoolDown > 0)
		m_TargetToKillCoolDown--;

	if((m_TargetToKillCoolDown == 0 && m_TargetToKill == -1))
	{
		int m_aTargetList[MAX_CLIENTS];
		int NbTargets = 0;
		int infectedCount = 0;
		for(int i=0; i<MAX_CLIENTS; i++)
		{
			if(m_apPlayers[i] && m_apPlayers[i]->IsZombie() && m_apPlayers[i]->GetClass() != PLAYERCLASS_UNDEAD)
			{
				if (m_apPlayers[i]->GetCharacter() && (m_apPlayers[i]->GetCharacter()->GetInfZoneTick()*Server()->TickSpeed()) < 1000*g_Config.m_InfNinjaTargetAfkTime) // Make sure zombie is not camping in InfZone
				{
					m_aTargetList[NbTargets] = i;
					NbTargets++;
				}
				infectedCount++;
			}
		}

		if(NbTargets > 0)
			m_TargetToKill = m_aTargetList[random_int(0, NbTargets-1)];

		if(m_TargetToKill == -1)
		{
			if (LastTarget >= 0)
				m_TargetToKill = LastTarget; // Reset Target if no new targets were found
		}

		if (infectedCount < g_Config.m_InfNinjaMinInfected)
		{
			m_TargetToKill = -1; // disable target system
		}
	}

	//Check for banvote
	if(!m_VoteCloseTime)
	{
		for(int i=0; i<MAX_CLIENTS; i++)
		{
			if(Server()->ClientShouldBeBanned(i))
			{
				char aDesc[VOTE_DESC_LENGTH] = {0};
				char aCmd[VOTE_CMD_LENGTH] = {0};
				str_format(aCmd, sizeof(aCmd), "ban %d %d Banned by vote", i, g_Config.m_SvVoteKickBantime*3);
				str_format(aDesc, sizeof(aDesc), "Ban \"%s\"", Server()->ClientName(i));
				m_VoteBanClientID = i;
				StartVote(aDesc, aCmd, "");
				continue;
			}
		}
	}

	//Check for mapVote
	if(!m_VoteCloseTime && m_pController->CanVote()) // there is currently no vote && its the start of a round
	{
		IServer::CMapVote* mapVote = Server()->GetMapVote();
		if (mapVote)
		{
			char aChatmsg[512] = {0};
			str_format(aChatmsg, sizeof(aChatmsg), "Starting vote '%s'", mapVote->m_pDesc);
			SendChat(-1, CGameContext::CHAT_ALL, aChatmsg);
			StartVote(mapVote->m_pDesc, mapVote->m_pCommand, mapVote->m_pReason);
		}
	}

	m_Collision.SetTime(m_pController->GetTime());

	//update hook protection in core
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i] && m_apPlayers[i]->GetCharacter())
		{
			m_apPlayers[i]->GetCharacter()->m_Core.m_Infected = m_apPlayers[i]->IsZombie();
			m_apPlayers[i]->GetCharacter()->m_Core.m_HookProtected = m_apPlayers[i]->HookProtectionEnabled();
		}
	}

	CGameContext::OnTick();

	int NumActivePlayers = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
		{
			if(m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				NumActivePlayers++;

			Server()->RoundStatistics()->UpdatePlayer(i, m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS);

			if(m_VoteLanguageTick[i] > 0)
			{
				if(m_VoteLanguageTick[i] == 1)
				{
					m_VoteLanguageTick[i] = 0;

					CNetMsg_Sv_VoteSet Msg;
					Msg.m_Timeout = 0;
					Msg.m_pDescription = "";
					Msg.m_pReason = "";
					Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);

					str_copy(m_VoteLanguage[i], "en", sizeof(m_VoteLanguage[i]));
				}
				else
				{
					m_VoteLanguageTick[i]--;
				}
			}
		}
	}

	//Check for new broadcast
	for(int i=0; i<MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
		{
			if(m_BroadcastStates[i].m_LifeSpanTick > 0 && m_BroadcastStates[i].m_TimedPriority > m_BroadcastStates[i].m_Priority)
			{
				str_copy(m_BroadcastStates[i].m_NextMessage, m_BroadcastStates[i].m_TimedMessage, sizeof(m_BroadcastStates[i].m_NextMessage));
			}

			//Send broadcast only if the message is different, or to fight auto-fading
			if(
				str_comp(m_BroadcastStates[i].m_PrevMessage, m_BroadcastStates[i].m_NextMessage) != 0 ||
				m_BroadcastStates[i].m_NoChangeTick > Server()->TickSpeed()
			)
			{
				CNetMsg_Sv_Broadcast Msg;
				Msg.m_pMessage = m_BroadcastStates[i].m_NextMessage;
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);

				str_copy(m_BroadcastStates[i].m_PrevMessage, m_BroadcastStates[i].m_NextMessage, sizeof(m_BroadcastStates[i].m_PrevMessage));

				m_BroadcastStates[i].m_NoChangeTick = 0;
			}
			else
				m_BroadcastStates[i].m_NoChangeTick++;

			//Update broadcast state
			if(m_BroadcastStates[i].m_LifeSpanTick > 0)
				m_BroadcastStates[i].m_LifeSpanTick--;

			if(m_BroadcastStates[i].m_LifeSpanTick <= 0)
			{
				m_BroadcastStates[i].m_TimedMessage[0] = 0;
				m_BroadcastStates[i].m_TimedPriority = BROADCAST_PRIORITY_LOWEST;
			}
			m_BroadcastStates[i].m_NextMessage[0] = 0;
			m_BroadcastStates[i].m_Priority = BROADCAST_PRIORITY_LOWEST;
		}
		else
		{
			m_BroadcastStates[i].m_NoChangeTick = 0;
			m_BroadcastStates[i].m_LifeSpanTick = 0;
			m_BroadcastStates[i].m_Priority = BROADCAST_PRIORITY_LOWEST;
			m_BroadcastStates[i].m_TimedPriority = BROADCAST_PRIORITY_LOWEST;
			m_BroadcastStates[i].m_PrevMessage[0] = 0;
			m_BroadcastStates[i].m_NextMessage[0] = 0;
			m_BroadcastStates[i].m_TimedMessage[0] = 0;
		}
	}

	Server()->RoundStatistics()->UpdateNumberOfPlayers(NumActivePlayers);

	//Clean old dots
	int DotIter;

	DotIter = 0;
	while(DotIter < m_LaserDots.size())
	{
		m_LaserDots[DotIter].m_LifeSpan--;
		if(m_LaserDots[DotIter].m_LifeSpan <= 0)
		{
			Server()->SnapFreeID(m_LaserDots[DotIter].m_SnapID);
			m_LaserDots.remove_index(DotIter);
		}
		else
			DotIter++;
	}

	DotIter = 0;
	while(DotIter < m_HammerDots.size())
	{
		m_HammerDots[DotIter].m_LifeSpan--;
		if(m_HammerDots[DotIter].m_LifeSpan <= 0)
		{
			Server()->SnapFreeID(m_HammerDots[DotIter].m_SnapID);
			m_HammerDots.remove_index(DotIter);
		}
		else
			DotIter++;
	}

	DotIter = 0;
	while(DotIter < m_LoveDots.size())
	{
		m_LoveDots[DotIter].m_LifeSpan--;
		m_LoveDots[DotIter].m_Pos.y -= 5.0f;
		if(m_LoveDots[DotIter].m_LifeSpan <= 0)
		{
			Server()->SnapFreeID(m_LoveDots[DotIter].m_SnapID);
			m_LoveDots.remove_index(DotIter);
		}
		else
			DotIter++;
	}
}

IGameServer *CreateGameServer() { return new CGameContextMod; }
