/* ==========================================================
 * d2warden
 * https://github.com/lolet/d2warden
 * ==========================================================
 * Copyright 2011-2013 Bartosz Jankowski
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * ========================================================== */

#include "stdafx.h"
#include "D2Handlers.h"

#include "D2Warden.h"
#include "LSpectator.h"
#include "LRoster.h"
#include "LItems.h"
#include "LEvents.h"
#include "LMonsters.h"
#include "LWorldEvent.h"
#include "process.h"
#include "RC4.h"

#include "Build.h"

map<string, int> ServerHashMap;

Room1* __stdcall D2GAME_PortalCrashFix(Act* ptAct, int LevelNo, int Unk0, int* xPos, int* yPos, int UnitAlign)
{
	Room1* ptRoom = D2Funcs.D2COMMON_GetRoomXYByLevel(ptAct, LevelNo, Unk0, xPos, yPos, UnitAlign);
	return ptRoom;

}

void  __stdcall OnLastHit(UnitAny* ptKiller, UnitAny * ptVictim, Damage * ptDamage)
{
	if (ptKiller->dwType == UNIT_PLAYER && ptVictim->dwType == UNIT_PLAYER) {
		if (D2Funcs.D2COMMON_GetStatSigned(ptVictim, STAT_HP, 0) <= 0)
		{
			int Dmg = (int)ptDamage->DamageTotal >> 8;
			if (Dmg > 50000) return;
			if (Dmg > ptKiller->pGame->DmgRekord) {
				ostringstream DmgStr;
				DmgStr << Dmg << "!";
				SendExEvent(ptKiller->pPlayerData->pClientData, COL_RED, D2EX_HOLYSHIT, 3, 150, -1, DmgStr.str(), DmgStr.str());
				if (!ptKiller->pGame->szRekordHolder[0]) {
					ostringstream EngMsg; EngMsg << ptKiller->pPlayerData->pClientData->AccountName << " has set a new damage record! (" << Dmg << " hp)";
					ostringstream PolMsg; PolMsg << ptKiller->pPlayerData->pClientData->AccountName << " ustanowil nowy rekord obrazen! (" << Dmg << " hp)";
					BroadcastEventMsgEx(ptKiller->pGame, COL_YELLOW, EngMsg.str(), PolMsg.str());
				}
				else
				{
					ostringstream EngMsg; EngMsg << ptKiller->pPlayerData->pClientData->AccountName << " has set a new damage record! (" << Dmg << " hp). Previous record belonged to " << ptKiller->pGame->szRekordHolder << " (" << ptKiller->pGame->DmgRekord << ')';
					ostringstream PolMsg; PolMsg << ptKiller->pPlayerData->pClientData->AccountName << " ustanowil nowy rekord obrazen! (" << Dmg << " hp). Poprzedni rekord nalezal do " << ptKiller->pGame->szRekordHolder << " (" << ptKiller->pGame->DmgRekord << ')';
					BroadcastEventMsgEx(ptKiller->pGame, COL_YELLOW, EngMsg.str(), PolMsg.str());
				}

				BroadcastExEvent(ptKiller->pGame, COL_WHITE, ptKiller->dwUnitId, 0, "data\\D2Ex\\Blobs");

				ptKiller->pGame->DmgRekord = Dmg;
				strcpy_s(ptKiller->pGame->szRekordHolder, 16, ptKiller->pPlayerData->pClientData->AccountName);
			}


			if ((GetTickCount() - ptVictim->pPlayerData->LastDamageTick < 5000) && ptVictim->pPlayerData->LastDamageId != 0 && ptVictim->pPlayerData->LastDamageId != ptKiller->dwUnitId) {
				UnitAny* pAssister = D2ASMFuncs::D2GAME_FindUnit(ptKiller->pGame, ptVictim->pPlayerData->LastDamageId, UNIT_PLAYER);
				if (pAssister) {
					LRost::UpdateRoster(ptKiller->pGame, pAssister->pPlayerData->szName, 3);
					LRoster * AssRoster = LRost::Find(ptKiller->pGame, pAssister->pPlayerData->szName);
					if (AssRoster) LRost::SyncClient(ptKiller->pGame, ptVictim->pPlayerData->LastDamageId, AssRoster);
					BroadcastExEvent(ptKiller->pGame, COL_WHITE, ptVictim->pPlayerData->LastDamageId, 5, "data\\D2Ex\\Blobs");
				}
			}
			ptVictim->pPlayerData->LastDamage = 0;
			ptVictim->pPlayerData->LastDamageId = 0;
			ptVictim->pPlayerData->LastDamageTick = 0;
		}
		else {
			int Percent = ((((int)ptDamage->DamageTotal >> 8) * 100) / (D2Funcs.D2COMMON_GetStatSigned(ptVictim, STAT_MAXHP, 0) >> 8));
			if (ptVictim->pPlayerData->LastDamageTick &&
				(GetTickCount() - ptVictim->pPlayerData->LastDamageTick < 5000)) ptVictim->pPlayerData->LastDamage += Percent; else	ptVictim->pPlayerData->LastDamage = Percent;
			ptVictim->pPlayerData->LastDamageId = ptKiller->dwUnitId;
			ptVictim->pPlayerData->LastDamageTick = GetTickCount();
		}
	}
}

void __fastcall DAMAGE_FireEnchanted(Game *pGame, UnitAny *pUnit, int a4, int a5)
{
	//Log("a4 =%d, a5 =%d",a4,a5);
}

int __stdcall OnCreateCorpse(Game *pGame, UnitAny *pUnit, int xPos, int yPos, Room1 *pRoom)
{
	int aX, aY, aLevel;
	Room1* aRoom;

	aLevel = D2Funcs.D2COMMON_GetTownLevel(pUnit->dwAct);
	aRoom = D2Funcs.D2COMMON_GetRoomXYByLevel(pRoom->pAct, aLevel, 0, &aX, &aY, 2);
	return D2Funcs.D2GAME_CreateCorpse(pGame, pUnit, aX, aY, aRoom);

}


int __stdcall GetItemCost(UnitAny *pPlayer, UnitAny *ptItem, int DiffLvl, QuestFlags *pQuestFlags, int NpcClassId, int InvPage)
{
	//if(pPlayer->pGame, pPlayer->pGame->dwGameState!=1)
	//return D2Funcs.D2COMMON_GetItemCost(pPlayer, ptItem, DiffLvl, pQuestFlags, NpcClassId, InvPage);
	//else
	return 1;
}

BOOL __stdcall isPermStore(Game* ptGame, UnitAny* ptNPC, UnitAny* ptItem)
{
	Npc* ptVendor = ptGame->pNpcControl->pNpc;

	if (!ptNPC || !ptVendor)
	{
		DEBUGMSG("isPermStore: null ptNPC or ptVendor!");
		return FALSE;
	}

	for (int i = 0; i < 64; i++)
	{
		if (ptVendor[i].npcNo == ptNPC->dwClassId) { ptVendor = &ptVendor[i]; break; }
		if (i == 63) { ptVendor = 0; break; }
	}

	if (ptVendor)
	{
		DWORD iCode = D2Funcs.D2COMMON_GetItemCode(ptItem);
		//WORLD EVENT
		if (wcfgEnableWE && WE_isKey(ptItem))
		{
			if (SellCount == NextDC) { WE_GenerateNextDC(); WE_CreateDCKey(ptNPC); }

			ptItem->pItemData->InvPage = 0xFF;
			return TRUE;
		}
		//OLD CODE
		if (ptGame->DifficultyLevel && (iCode == ' 4ph' || iCode == ' 5ph' || iCode == ' 4pm' || iCode == ' 5pm')) return TRUE;

		for (unsigned int i = 0; i < ptVendor->nPermStoreRecords; i++)
		{
			if (ptVendor->pPermStoreList[i] == iCode) return 1;
		}
	}

	return FALSE;
}


void __fastcall OnMonsterDeath(UnitAny* ptKiller, UnitAny * ptVictim, Game * ptGame)
{
	DEBUGMSG(__FUNCTION__)
		if (!ptVictim || !ptKiller) return;
	if (ptKiller->dwType) return;


	////TO MOze crashoac
	//if(ptKiller->dwType==0) 
	//LRost::UpdateRoster(ptGame,ptKiller->pPlayerData->szName,0);
	//LRost::UpdateRoster(ptGame,ptVictim->pPlayerData->szName,1);

	//LRost::SendKills(ptGame);

}


void __fastcall OnNPCHeal(UnitAny* pUnit)
{
	StatList* pList = D2Funcs.D2COMMON_GetStateStatList(pUnit, 1); // FREEZE
	if (pList)
	{
		D2Funcs.D2COMMON_RemoveStatList(pUnit, pList);
		D2Funcs.D2COMMON_FreeStatList(pList);
	}
	pList = D2Funcs.D2COMMON_GetStateStatList(pUnit, 2); // PSN
	if (pList)
	{
		D2Funcs.D2COMMON_RemoveStatList(pUnit, pList);
		D2Funcs.D2COMMON_FreeStatList(pList);
	}
	pList = D2Funcs.D2COMMON_GetStateStatList(pUnit, 54); // UNINTERUPTABLE
	if (pList)
	{
		D2Funcs.D2COMMON_RemoveStatList(pUnit, pList);
		D2Funcs.D2COMMON_FreeStatList(pList);
	}
	pList = D2Funcs.D2COMMON_GetStateStatList(pUnit, 87); // SLOW MISSILE
	if (pList)
	{
		D2Funcs.D2COMMON_RemoveStatList(pUnit, pList);
		D2Funcs.D2COMMON_FreeStatList(pList);
	}

	if (pUnit->pGame, pUnit->pGame->dwGameState == 1) //FFA MODE
		CreateFFAItems(pUnit);


	if (pUnit->pGame, pUnit->pGame->bFestivalMode == 1)
		if (pUnit->pPlayerData->isPlaying)
		{
			pUnit->pPlayerData->CanAttack = 0;
			pUnit->pPlayerData->SaidGO = 0;

			DoRoundEndStuff(pUnit->pGame, pUnit);

			pUnit->pPlayerData->isPlaying = 0;
			pUnit->pGame->nPlaying--;
		}


}

DWORD __fastcall OnD2ExPacket(Game* ptGame, UnitAny* ptPlayer, BYTE *ptPacket, DWORD PacketLen) // Zostawione do wstecznej kompatybilnosci
{
	if (PacketLen != 5) return 3;
	DWORD Version = *(DWORD*)&ptPacket[1];
	//Log("Otrzymano pakiet wersji!");
	SendMsgToClient(ptPlayer->pPlayerData->pClientData, ptPlayer->pPlayerData->pClientData->LocaleID == 10 ? "Uwaga masz starego klienta! Wpisz #update w celu aktualizacji" : "Warning, your client is outdated! Type #update to fix this!");
	return 0;
	//if(Version==1)
	//{
	//WardenClient_i ptCurrentClient = GetClientByID(ptPlayer->pPlayerData->pClientData->ClientID);
	//if(ptCurrentClient == hWarden.Clients.end()) return 0;
	//ptCurrentClient->NewPatch=1;
	//UNLOCK
	//return 0;
	//}
	//return 3;
}



DWORD __fastcall SetDRCap(Attack *pAttack) //Unused atm but will be
{
	int DR = 50;
	//	
	//if(pAttack->dwAttackerType==0 && pAttack->dwDefenderType==0 && ( pAttack->ptDamage->ResultFlags & RESULTFLAG_DEATH))
	//{ 
	//	ExEvent hEvent = {0};
	//	hEvent.MsgType = 1;
	//	hEvent.Color = COL_RED;
	//	hEvent.wX = -1;
	//	hEvent.wY = 50;
	//	hEvent.Argument = 2;
	//	hEvent.Sound = D2EX_HOLYSHIT;
	//	hEvent.P_A6 = 0xA6;
	//	int Dmg = pAttack->ptDamage->PhysicalDamage +  pAttack->ptDamage->FireDamage +  pAttack->ptDamage->ColdDamage +  pAttack->ptDamage->LightningDamage +  pAttack->ptDamage->MagicDamage;
	//	sprintf_s(hEvent.szMsg,255,"%d !",Dmg);
	//	hEvent.PacketLen = 0xE + strlen(hEvent.szMsg) +1;
	//	BroadcastPacket(pAttack->pGame,(BYTE*)&hEvent,hEvent.PacketLen);
	//	SendMsgToClient(pAttack->pAttacker->pPlayerData->pClientData,"Last hit : %d",Dmg);
	//}

	return DR;
}

Act* __stdcall OnActLoad(DWORD ActNumber, DWORD InitSeed, DWORD Unk0, Game *pGame, DWORD DiffLvl, DWORD* pMemPool, DWORD TownLevelId, DWORD Func1, DWORD Func2)
{
	int MySeed = 0;
	BYTE MyDiff = (BYTE)DiffLvl;
	if (strlen(pGame->GameDesc) > 0)
	{
		char *nt = 0;
		char tk[32];
		strcpy_s(tk, 32, pGame->GameDesc);
		char * ret = strtok_s(tk, "- ", &nt);
		while (ret)
		{
			if (ret[0] == 'm' && strlen(ret) > 1 && wcfgEnableSeed)
				MySeed = atoi(ret + 1);
			if (ret[0] == 't' && strlen(ret) == 1 && wcfgAllowTourMode)
				pGame->bFestivalMode = 1;
			if (_strnicmp(ret, "ffa", 3) == 0 && wcfgFFAMode)
				pGame->dwGameState = 1;
			ret = strtok_s(NULL, "- ", &nt);
		}
	}
	if (MySeed)
	{
		pGame->InitSeed = MySeed;
		return D2Funcs.D2COMMON_LoadAct(ActNumber, MySeed, Unk0, pGame, MyDiff, pMemPool, TownLevelId, Func1, Func2);
	}
	return D2Funcs.D2COMMON_LoadAct(ActNumber, InitSeed, Unk0, pGame, MyDiff, pMemPool, TownLevelId, Func1, Func2);
}



BOOL __fastcall OnReceivePacket(BYTE * ThePacket, PacketData * pClient) // return is currently ignored
{
	if (!pClient) return true;
	int ClientID = pClient->ClientID;

	switch (ThePacket[0])
	{
	case 0x1A: //EQUIP CHECK
	case 0x1D:
	case 0x16:
	{
		break;
		// 1a   9   Equip item         1a [DWORD id] [WORD position] 00 00
		DWORD ItemID = 0;
		DWORD Socket = D2Funcs.D2NET_GetClient(ClientID);
		if (!Socket) break;
		Game* pGame = D2Funcs.D2GAME_GetGameByNetSocket(Socket);
		if (!pGame) break;
		ClientData* ptClientData = FindClientDataById(pGame, ClientID);
		if (!ptClientData) { D2ASMFuncs::D2GAME_LeaveCriticalSection(pGame); break; }

		if (ThePacket[0] == 0x16)
			ItemID = *(DWORD*)&ThePacket[5];
		else
			ItemID = *(DWORD*)&ThePacket[1];
		UnitAny* ptItem = D2ASMFuncs::D2GAME_FindUnit(ptClientData->pGame, ItemID, 4);
		if (ptItem)
		{
			if (!ptItem->pItemData->dwItemFlags.bPersonalized) { D2ASMFuncs::D2GAME_LeaveCriticalSection(pGame); break; }

			if (!isAnAdmin(ptClientData->AccountName))
				if (ptClientData->AccountName != ptItem->pItemData->szPlayerName)
				{
					SendMsgToClient(ptClientData, ptClientData->LocaleID == LOC_PL ? "Nie mozesz zalozyc przedmiotu, ktory nie nalezy do ciebie!" : "You cant equip item bound to other player!");
					*(DWORD*)&ThePacket[1] = 0;
				}
		}
		D2ASMFuncs::D2GAME_LeaveCriticalSection(pGame);
	}
		break;
	case 0x1F://STASH HACK
	{
		//GC 78:   0x1F SwapContainerItem; SubjectUID: 28; ObjectUID: 29; X: 5; Y: 0
		//GC 78:   17   1f [1c 00 00 00] [1d 00 00 00] [05 00 00 00] [00 00 00 00]
		DWORD Socket = D2Funcs.D2NET_GetClient(ClientID);
		if (!Socket) break;
		Game* pGame = D2Funcs.D2GAME_GetGameByNetSocket(Socket);
		if (!pGame) break;
		ClientData* ptClientData = FindClientDataById(pGame, ClientID);
		if (!ptClientData) { D2ASMFuncs::D2GAME_LeaveCriticalSection(pGame); break; }

		int LvlNo = D2Funcs.D2COMMON_GetLevelNoByRoom(ptClientData->ptRoom);
		if (LvlNo == 0) { D2ASMFuncs::D2GAME_LeaveCriticalSection(pGame); break; }
		int ActNo = D2Funcs.D2COMMON_GetActNoByLevelNo(LvlNo);
		if (LvlNo == D2Funcs.D2COMMON_GetTownLevel(ActNo)) { D2ASMFuncs::D2GAME_LeaveCriticalSection(pGame); break; }
		DWORD ItemID = *(DWORD*)&ThePacket[5];
		UnitAny* ptItem = D2ASMFuncs::D2GAME_FindUnit(ptClientData->pGame, ItemID, UNIT_ITEM);
		if (!ptItem) { D2ASMFuncs::D2GAME_LeaveCriticalSection(pGame); break; }
		if (ptItem->pItemData->InvPage == 4) Log("HACK: %s (*%s) opened stash being out of town [STASH HACK]!", ptClientData->CharName, ptClientData->AccountName);
		D2ASMFuncs::D2GAME_LeaveCriticalSection(pGame);
	}
		break;
	}
	return true;
}


//Valid for 0x06*, 0x07, 0x09*, 0x0A,  || 0x0D*, 0x0E, 0x10*, 0x11
DWORD __fastcall OnClickUnit(Game* ptGame, UnitAny* ptPlayer, SkillTargetPacket *ptPacket, DWORD PacketLen)
{
	int InRange = 0;
	if (PacketLen != 9) return 3;
	if (!ptGame) return 3;
	if (ptPlayer->dwType != UNIT_PLAYER) return 3;
	if (ptPacket->UnitType > 5) return 3;

	InRange = D2ASMFuncs::D2GAME_isUnitInRange(ptGame, ptPacket->UnitId, ptPacket->UnitType, ptPlayer, 50);
	if (InRange == 2)
	{
		DEBUGMSG("OnClickUnit: Over the range (3)")
			return 2;
	}
	if (InRange == 3)
	{
		DEBUGMSG("OnClickUnit: Over the range (3)");
		return 3;
	}
	Skill * ptSkill = (ptPacket->Header == 6 || ptPacket->Header == 7 || ptPacket->Header == 9 || ptPacket->Header == 0xA) ? D2Funcs.D2COMMON_GetLeftSkill(ptPlayer) : D2Funcs.D2COMMON_GetRightSkill(ptPlayer);
	if (!ptSkill)
	{
		DEBUGMSG("%s: ptSkill not found!. Packet id is = %d", __FUNCTION__, ptPacket->Header);
		return 3;
	}
	int SkillId = D2Funcs.D2COMMON_GetSkillId(ptSkill, __FILE__, __LINE__);

	PlayerData* pPlayerData = ptPlayer->pPlayerData;
	if (!pPlayerData) return 2;

	if (TeleChars[ptPlayer->dwClassId] == FALSE && ptPlayer->pGame->dwGameState == 0 && SkillId == 0x36)
	{
		SendMsgToClient(pPlayerData->pClientData, pPlayerData->pClientData->LocaleID == 10 ? "Teleport nie jest dozwolony dla tej klasy!" : "Teleport Is Not Allowed For This Character");
		return 0;
	}

	if (SkillId == 0x65 && !wcfgAllowHB)
	{
		SendMsgToClient(pPlayerData->pClientData, pPlayerData->pClientData->LocaleID == 10 ? "Swiety pocisk jest zabroniony na tym serwerze" : "Holy Bolt Is Not Allowed On This Server");
		return 0;
	}
	if (SkillId == 151 && !wcfgAllowNLWW)
	{
		SendMsgToClient(pPlayerData->pClientData, pPlayerData->pClientData->LocaleID == 10 ? "NLWW jest zabronione na tym serwerze" : "NLWW Is Not Allowed On This Server");
		return 0;
	}

	static int AttackCount;

	if (ptGame->bFestivalMode == 1 && pPlayerData->CanAttack == 0 && (!isSafeSkill(SkillId) && SkillId != D2S_CHARGE))
	{
		if (AttackCount == 0)
		{
			SendMsgToClient(pPlayerData->pClientData, pPlayerData->pClientData->LocaleID == 10 ? "Wpisz #go aby rozpoczac runde!" : "Type #go to start round");
		}
		AttackCount++;
		if (AttackCount > 4) AttackCount = 0;
		return 0;
	}


	int nPierceIdx = D2Funcs.D2COMMON_GetBaseStatSigned(ptPlayer, 328, 0);
	D2Funcs.D2COMMON_SetStat(ptPlayer, 328, nPierceIdx + 1, 0);

	D2ASMFuncs::D2GAME_CastSkillOnUnit(ptPlayer, ptSkill, ptGame, ptPacket->UnitType, ptPacket->UnitId, (ptPacket->Header == 6 || ptPacket->Header == 9 || ptPacket->Header == 0xD || ptPacket->Header == 0x10) ? 1 : 0);
	return 0;
}


DWORD __fastcall OnClickLoc(Game* ptGame, UnitAny* ptPlayer, SkillPacket *ptPacket, DWORD PacketLen)
{
	bool InRange = false;
	static int AttackCount;
	if (PacketLen != 5) return 3; // zwroc hack
	if (!ptGame) return 3;
	if (ptPlayer->dwType != UNIT_PLAYER) return 3;

	WORD UnitX = D2Funcs.D2GAME_GetUnitX(ptPlayer);
	WORD UnitY = D2Funcs.D2GAME_GetUnitY(ptPlayer);

	int xOffset = UnitX - ptPacket->xPos;
	if (xOffset < 0) xOffset = -xOffset;
	int yOffset = UnitY - ptPacket->yPos;
	if (yOffset < 0) yOffset = -yOffset;

	if (yOffset < 50 && xOffset < 50) InRange = true;

	PlayerData* pPlayerData = ptPlayer->pPlayerData;
	if (!pPlayerData)
	{
		DEBUGMSG("Didn't find a PlayerData, function %s, %d", __FUNCTION__, __LINE__);
		return 2;
	}

	if (InRange)
	{
		pPlayerData->GameFrame = ptGame->GameFrame;
		Skill * ptSkill = (ptPacket->Header == 5 || ptPacket->Header == 8) ? D2Funcs.D2COMMON_GetLeftSkill(ptPlayer) : D2Funcs.D2COMMON_GetRightSkill(ptPlayer);
		if (!ptSkill) return 3;
		int SkillId = D2Funcs.D2COMMON_GetSkillId(ptSkill, __FILE__, __LINE__);

		if (TeleChars[ptPlayer->dwClassId] == FALSE && ptPlayer->pGame->dwGameState == 0 && SkillId == 0x36)
		{
			SendMsgToClient(pPlayerData->pClientData, pPlayerData->pClientData->LocaleID == 10 ? "Teleport nie jest dozwolony dla tej klasy!" : "Teleport Is Not Allowed For This Character");
			return 0;
		}

		if (SkillId == 0x65 && !wcfgAllowHB) {
			SendMsgToClient(pPlayerData->pClientData, pPlayerData->pClientData->LocaleID == 10 ? "Swiety pocisk jest zabroniony na tym serwerze" : "Holy Bolt Is Not Allowed On This Server");
			return 0;
		}

		int nPierceIdx = D2Funcs.D2COMMON_GetBaseStatSigned(ptPlayer, 328, 0);
		D2Funcs.D2COMMON_SetStat(ptPlayer, 328, nPierceIdx + 1, 0);

		if (ptGame->bFestivalMode && !pPlayerData->CanAttack && !isSafeSkill(SkillId))
		{
			if (AttackCount == 0)
			{
				SendMsgToClient(pPlayerData->pClientData, pPlayerData->pClientData->LocaleID == 10 ? "Wpisz #go aby rozpoczac runde!" : "Type #go to start round");
			}
			AttackCount++;
			if (AttackCount > 4)
				AttackCount = 0;
			return 0;
		}

		D2ASMFuncs::D2GAME_CastSkill(ptPlayer, ptSkill, ptGame, ptPacket->xPos, ptPacket->yPos);

		if (!wcfgDetectTrick)
			return 0;

		WardenClient_i ptWardenClient = GetClientByID(pPlayerData->pClientData->ClientID);
		if (ptWardenClient == hWarden.Clients.end()) return 0;

		if (GetTickCount() > ptWardenClient->UIModesTime + 500) { UNLOCK return 0; }

		if ((ptWardenClient->UIModes[UI_CHARACTER] || ptWardenClient->UIModes[UI_QUEST]) && (ptWardenClient->MouseXPosition >= 0 && ptWardenClient->MouseXPosition <= 200) && (ptWardenClient->MouseYPosition >= 0 && ptWardenClient->MouseYPosition <= 550))
		{
			if (ptWardenClient->DebugTrick)
				SendMsgToClient(ptWardenClient->ptClientData, "Trick (Left window) X=%d Y=%d, LAG= %d ms", ptWardenClient->MouseXPosition, ptWardenClient->MouseYPosition, GetTickCount() - ptWardenClient->UIModesTime);
			Log("HACK: %s (*%s) used Polish GA Trick [%s]!, skill : %s XY=[%d,%d]", ptWardenClient->CharName.c_str(), ptWardenClient->AccountName.c_str(), ptWardenClient->UIModes[UI_CHARACTER] ? "Character Stats" : "Quests", ConvertSkill(SkillId).c_str(), ptWardenClient->MouseXPosition, ptWardenClient->MouseYPosition);
		}
		else
			if ((ptWardenClient->UIModes[UI_INVENTORY] || ptWardenClient->UIModes[UI_SKILL]) && (ptWardenClient->MouseXPosition >= 600 && ptWardenClient->MouseXPosition <= 800) && (ptWardenClient->MouseYPosition >= 0 && ptWardenClient->MouseYPosition <= 550))
			{
				if (ptWardenClient->DebugTrick)
					SendMsgToClient(ptWardenClient->ptClientData, "Trick (Right window) X=%d Y=%d, LAG = %d ms", ptWardenClient->MouseXPosition, ptWardenClient->MouseYPosition, GetTickCount() - ptWardenClient->UIModesTime);
				Log("HACK: %s (*%s) used Polish GA Trick [%s]!, skill : %s XY=[%d,%d]", ptWardenClient->CharName.c_str(), ptWardenClient->AccountName.c_str(), ptWardenClient->UIModes[UI_INVENTORY] ? "Inventory" : "Skill Tree", ConvertSkill(SkillId).c_str(), ptWardenClient->MouseXPosition, ptWardenClient->MouseYPosition);
			}
		UNLOCK
			return 0;
	}
	else
	{
		if ((signed int)(ptGame->GameFrame - pPlayerData->GameFrame) > 25)
		{
			ClientData * pClient = pPlayerData->pClientData;
			if (pClient)
			{
				ReassignPacket hReassign = { 0 };
				hReassign.Header = 0x15;
				hReassign.UnitId = ptPlayer->dwUnitId;
				hReassign.xPos = UnitX;
				hReassign.yPos = UnitY;
				hReassign.Reassign = 1;

				D2ASMFuncs::D2GAME_SendPacket(pClient, (BYTE*)&hReassign, 11);
			}
		}
		return 1;
	}
	return 3;
}

int __fastcall OnGameEnter(ClientData* pClient, Game* ptGame, UnitAny* ptPlayer)
{
	//if(pClient->InitStatus!=4)
	//Log("NowyKlient: -> %s, InitStatus == %d",pClient->CharName,pClient->InitStatus);

	//LRost::SendKills(Data->ptGame);
	//LRost::SendDeaths(Data->ptGame);

	ExEventDownload pEvent;
	::memset(&pEvent, 0, sizeof(ExEventDownload));
	pEvent.P_A6 = 0xA6;
	pEvent.MsgType = EXEVENT_DOWNLOAD;
	strcpy_s(pEvent.szURL, 255, wcfgClansURL.c_str());
	if (pEvent.szURL[0])
		pEvent.PacketLen = 14 + strlen(pEvent.szURL) + 1;
	else
		pEvent.PacketLen = 15;
	D2ASMFuncs::D2GAME_SendPacket(pClient, (BYTE*)&pEvent, pEvent.PacketLen);

	if (strlen(ptGame->GameDesc) > 0)
	{
		char tk[32];
		strcpy_s(tk, 32, ptGame->GameDesc);
		char *nt = 0;
		for (char * ret = strtok_s(tk, " -", &nt); ret; ret = strtok_s(NULL, " -", &nt))
		{
			if (_strnicmp(ret, "ffa", 3) == 0) { SendMsgToClient(pClient, pClient->LocaleID == 10 ? "Tryb FFA jest w��czony na tej grze!" : "Free For All Mode Is Enabled!"); continue; }
			if (_strnicmp(ret, "m", 1) == 0 && strlen(ret) > 1 && wcfgAllowTourMode) { SendMsgToClient(pClient, pClient->LocaleID == 10 ? "Ustawiono identyfikator mapy na '%d'" : "Custom Map Id : '%d'", atoi(ret + 1)); continue; }
			if (_strnicmp(ret, "t", 1) == 0 && strlen(ret) == 1) { SendMsgToClient(pClient, pClient->LocaleID == 10 ? "�c;Tryb turniejowy!" : "�c;Tournament Mode!"); continue; }
		}
	}

	unsigned char RC4_KEY_0X66[16], RC4_KEY_0XAE[16];

	DEBUGMSG("NEWCLIENT: Trying to add  '%s' !", pClient->CharName);

	if (ServerHashMap.count(pClient->CharName))
	{
		WardenClient NewClientData = { 0 };

		NewClientData.ClientID = pClient->ClientID;
		memcpy(NewClientData.SessionKey, &ServerHashMap[pClient->CharName], 4);
		*(DWORD*)&NewClientData.SessionKey = ServerHashMap[pClient->CharName];
		ServerHashMap.erase(pClient->CharName);
		NewClientData.ClientLogonTime = GetTickCount();
		NewClientData.NextCheckTime = NewClientData.ClientLogonTime + 500;
		NewClientData.AccountName = pClient->AccountName;
		NewClientData.CharName = pClient->CharName;
		NewClientData.ptClientData = pClient;
		NewClientData.ptGame = ptGame;
		NewClientData.ptPlayer = ptPlayer;
		NewClientData.MyIp = "";
		NewClientData.MouseXPosition = 400; //neutralne pozycje
		NewClientData.MouseYPosition = 300; //j.w.

		HashGameSeed(RC4_KEY_0XAE, RC4_KEY_0X66, NewClientData.SessionKey, 4);
		rc4_setup(NewClientData.RC4_KEY_0XAE, RC4_KEY_0XAE, 16);
		rc4_setup(NewClientData.RC4_KEY_0X66, RC4_KEY_0X66, 16);

		LOCK
			DEBUGMSG("Added (*%s) %s to WardenQueue", pClient->AccountName, pClient->CharName);
		hWarden.Clients.push_back(NewClientData);
		UNLOCK
			if (hWarden.Clients.size() > 600)
			{
#ifdef _ENGLISH_LOGS
				Log("NEWCLIENT: Number of clients (%d) is bigger than 600, isn't it a memory leak though?", hWarden.Clients.size());
#else
				Log("NOWYKLIENT: Liczba klient�w w petli %d wieksza niz 600, czy to napewno nie wyciek pamieci?", hWarden.Clients.size());
#endif
			}
		DEBUGMSG("Player %s has been added to WardenQueue!", pClient->CharName);
	}
	else
	{
#ifdef _ENGLISH_LOGS
		Log("NEWCLIENT: No SessionKey in database! Dropping player %s !", pClient->AccountName);
#else
		Log("NOWYKLIENT: Brak SessionKey w bazie! Wykopuje gracza %s !", pClient->AccountName);
#endif
		KickPlayer(pClient->ClientID);
	}
	DEBUGMSG("Triggering the event from OnGameJoin..");
	SetEvent(hWardenCheckEvent);


	return 0;
}

int  __fastcall d2warden_0X66Handler(Game* ptGame, UnitAny* ptPlayer, BYTE *ptPacket, int PacketLen) // Pakiet 66 -> Odpowiedz klienta na zapytanie Wardena
{
	// Mozliwe wartosci zwrotu kazdego pakietu:
	// 0 - wszystko ok
	// 1 - ?
	// 2 - blad
	// 3 - hack 
	if (!ptPlayer)
	{
		DEBUGMSG("WardenPacket: ptPlayer == null!");
		return 3;
	}

	DWORD ClientID = ptPlayer->pPlayerData->pClientData->ClientID;

	if (PacketLen < 3)
	{
		DEBUGMSG("WardenPacket: PacketLen < 3 !");
		return 3;
	}

	if (!ClientID)
	{
		DEBUGMSG("WardenPacket: No client id!");
		return 3;
	}


	WardenClient_i i = GetClientByID(ClientID);
	if (i != hWarden.Clients.end())
	{
		i->pWardenPacket.ReceiveTime = GetTickCount();
		i->pWardenPacket.PacketLen = ptPacket[2] * 256 + ptPacket[1];

		if (i->pWardenPacket.PacketLen == 0 || i->pWardenPacket.PacketLen > 512) // Taka jest maksymalna wielkosc pakietu obslugiowanego przez d2
		{
			DEBUGMSG("WardenPacket: Packet size exceeds 512 bytes!");
			return 3;
		}

		BYTE *ThePacket = new BYTE[i->pWardenPacket.PacketLen];
		if (!ThePacket)
		{
			Log("WardenPacket: No memory to allocate packet data!");
			return 3;
		}

		memcpy(ThePacket, ptPacket + 3, i->pWardenPacket.PacketLen);
		i->pWardenPacket.ThePacket = ThePacket;

		rc4_crypt(i->RC4_KEY_0X66, i->pWardenPacket.ThePacket, i->pWardenPacket.PacketLen);
		//DEBUGMSG("WardenPacket: Received answer in %d ms", i->pWardenPacket.SendTime ? (i->pWardenPacket.ReceiveTime - i->pWardenPacket.SendTime) : 0);
		i->NextCheckTime = GetTickCount();
		UNLOCK
		//DEBUGMSG("WardenPacket: Triggering the check event...");
		SetEvent(hWardenCheckEvent);
		return 0; // Wszystko OK!
	}
	else
	{
		DEBUGMSG("WardenPacket: Client %d, %s (*%s) is not in WardenQueue!!", ClientID, ptPlayer->pPlayerData->pClientData->CharName, ptPlayer->pPlayerData->pClientData->AccountName);

#ifdef _ENGLISH_LOGS
		Log("WardenPacket: Unexpected packet from player %s (*%s)! Returning an error..", ptPlayer->pPlayerData->szName, ptPlayer->pPlayerData->pClientData->AccountName);
#else
		Log("WardenPacket: Nieoczekiwany pakiet od gracza %s (*%s)! Zwracam blad...", ptPlayer->pPlayerData->szName, ptPlayer->pPlayerData->pClientData->AccountName);
#endif
		return 3;
	}

}


int  __fastcall d2warden_0X68Handler(PacketData *pPacket) // 0x68 pakiet -> Dodaj nowego klienta do petli Wardena
{
	// Moje badania :
#pragma pack(push, 1)
	struct px68
	{
		BYTE Header;			// 0x00
		DWORD ServerHash;		// 0x01 also SessionKey - used to keep BN connection alive
		WORD ServerToken;		// 0x05 TicketNo - increase every player join
		BYTE ClassId;			// 0x07
		DWORD VerByte;			// 0x08 (11 for 1.11) etc
		DWORD Unk1;				// 0x0C FOG_isExpansion_10227() != 0 ? 0xED5DCC50u : 0x2185EDD6u; (const)
		DWORD Unk2;				// 0x10 0x91A519B6 (const)
		BYTE LocaleId;			// 0x14 
		char szCharName[16];	// 0x15
	};
	struct px67 //Create Game 0x2E
	{
		BYTE Header;			// 0x00
		char szGameName[16];	// 0x01
		BYTE GameType;			// 0x11
		BYTE ClassId;			// 0x12
		BYTE ArenaUnk;			// 0x13
		BYTE DiffLvl;			// 0x14
		char szCharName[16];	// 0x15
		WORD Unk1;				// 0x25
		DWORD ArenaFlags;		// 0x27
		BYTE Unk2;				// 0x2B
		BYTE Unk3;				// 0x2C
		BYTE LocaleId;			// 0x2D
	};
#pragma pack(pop)


	//	unsigned char RC4_KEY_0X66[16],RC4_KEY_0XAE[16];
	px68* pJoinPacket = (px68*)pPacket->aPacket;
	px67* pCreatePacket = (px67*)pPacket->aPacket;

#ifdef _SINGLEPLAYER
	if(pJoinPacket->Header !=0x68 || pCreatePacket->Header !=0x67 ) return 0;
	if(pCreatePacket->Header == 0x67) 
	{
		ServerHashMap[pCreatePacket->szCharName] = 0;
		return 0;
	}
#endif


	int D2Version = pJoinPacket->VerByte;

	if (D2Version < 16 && !wcfgAllowVanilla) /// Zmiana na 14 11.04.11 . Zmiana na 15 08.07.11. Zmiana na 16 02.02.12
	{
		if (D2Version == 13)
#ifdef _ENGLISH_LOGS
			Log("NewClient: Dropping connection with '%s', reason : No D2Ex2 installed.",pJoinPacket->szCharName);
#else
			Log("NowyKlient: Zrywam polaczenie z graczem '%s', powod : Brak D2Ex.", pJoinPacket->szCharName);
#endif
		else
#ifdef _ENGLISH_LOGS
			Log("NewClient: Dropping connection with '%s', reason : Unsupported patch version (1.%d).",pJoinPacket->szCharName,D2Version);
#else
			Log("NowyKlient: Zrywam polaczenie z graczem '%s', powod : Nieoblsugiwana wersja patcha (1.%d).", pJoinPacket->szCharName, D2Version);
#endif
		KickPlayer(pPacket->ClientID);
		return 3;
	}
	ServerHashMap[pJoinPacket->szCharName] = pJoinPacket->ServerHash;

	return 0;
}

int __fastcall ReparseChat(Game* pGame, UnitAny *pUnit, BYTE *ThePacket, int PacketLen)
{
	char Msg[256];
	char text[256];
	WORD MsgLen = 0;
	char *szName = pUnit->pPlayerData->szName;
	int nNameLen = strlen(szName);
	strncpy_s(Msg, 255, (char*)ThePacket + 3, -1);
	Msg[255] = 0;
	BYTE * aPacket = 0;

	if (wcfgAllowLoggin) LogToFile("GameLog.txt", true, "%s\t%s\t%s\t%s", pGame->GameName, pUnit->pPlayerData->pClientData->AccountName, szName, Msg);

	if (nNameLen > 12)
	{
		_snprintf_s(text, 256, 256, "%s: �c%d%s", szName, COL_WHITE, Msg);

		MsgLen = 27 + strlen(text);
		aPacket = new BYTE[MsgLen];
		memset(aPacket, 0, MsgLen);
		aPacket[0] = 0x26;
		aPacket[1] = 0x04;
		aPacket[3] = 0x02; //MsgStyle
		aPacket[8] = GetColorNameByAcc(pUnit->pPlayerData->pClientData->AccountName);
		strcpy_s((char*)aPacket + 10, 16, "[administrator]");
		strcpy_s((char*)aPacket + 26, MsgLen - 26, text);
	}
	else
	{
		MsgLen = 12 + strlen(Msg) + nNameLen + 3;
		aPacket = new BYTE[MsgLen];
		memset(aPacket, 0, MsgLen);
		aPacket[0] = 0x26;
		aPacket[1] = 0x01;
		aPacket[3] = 0x02; //MsgStyle
		aPacket[9] = 0x53; //MsgFlags
		aPacket[10] = 0xFF;
		aPacket[11] = 'c';
		aPacket[12] = 0x30 + (BYTE)GetColorNameByAcc(pUnit->pPlayerData->pClientData->AccountName);
		strcpy_s((char*)aPacket + 13, nNameLen + 1, szName);
		strcpy_s((char*)aPacket + 14 + nNameLen, strlen(Msg) + 1, Msg);
	}

	ClientData * pClientList = pGame->pClientList;
	while (pClientList)
	{
		if (pClientList->InitStatus == 4) D2ASMFuncs::D2GAME_SendPacket(pClientList, aPacket, MsgLen);
		if (!pClientList->ptPrevious) break;
		pClientList = pClientList->ptPrevious;
	}

	delete[] aPacket;
	return 0;
}


BOOL __fastcall OnChat(UnitAny* pUnit, BYTE *ThePacket)
{
	int ClientID = pUnit->pPlayerData->pClientData->ClientID;

	if (ClientID == NULL) return TRUE;
	if (ThePacket[1] != 0x01) return TRUE;
	char Msg[500];
	strcpy_s(Msg, 500, (char*)ThePacket + 3);
	if (Msg[0] == '#')
	{
		char * str, *t;
		str = strtok_s(Msg, " ", &t);
		if (str)
		{
			if (!ParseItemsCmds(pUnit, str, t)) return false;
			if (!ParseMonCmds(pUnit, str, t)) return false;

			//if(_stricmp(str,"#players")==0)
			//{
			//str = strtok_s(NULL," ",&t);
			//if(!str) { SendMsgToClient(pUnit->pPlayerData->pClientData,"Type player count!"); return false;}
			//int NewPl = atoi(str);
			//SendMsgToClient(pUnit->pPlayerData->pClientData,"Prev players set from %d to %d.",PNo,NewPl);
			//PNo=NewPl;
			//return false;
			//}
			if (_stricmp(str, "#spec") == 0)
			{
				if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName)) return TRUE;

				str = strtok_s(NULL, " ", &t);
				if (!str) { SendMsgToClient(pUnit->pPlayerData->pClientData, "#spec <*account> or #spec [charname]!"); return false; }
				WardenClient_i psUnit = hWarden.Clients.end();
				if (str[0] == '*')
				{
					str++;
					if (!_stricmp(pUnit->pPlayerData->pClientData->AccountName, str)) { SendMsgToClient(pUnit->pPlayerData->pClientData, "You cannot spectator yourself!"); return false; }
					psUnit = GetClientByAcc(str);
					if (psUnit != hWarden.Clients.end())
						if (!_stricmp(pUnit->pPlayerData->pClientData->CharName, str))
						{
							SendMsgToClient(pUnit->pPlayerData->pClientData, "You cannot spectator yourself!");
							UNLOCK
								return false;
						}
				}
				else
				{
					if (!_stricmp(pUnit->pPlayerData->pClientData->CharName, str)) { SendMsgToClient(pUnit->pPlayerData->pClientData, "You cannot spectator yourself!"); return false; }
					psUnit = GetClientByName(str);
				}

				if (psUnit == hWarden.Clients.end()) { SendMsgToClient(pUnit->pPlayerData->pClientData, "Wrong charname or player is not in the game!"); return false; }
				if (pUnit->pGame != psUnit->ptGame) { SendMsgToClient(pUnit->pPlayerData->pClientData, "Player is not in the same game!"); UNLOCK return false; }
				//BroadcastMsg(pUnit->pPlayerData->pClientData->pGame,"'%s' has been kicked by *%s",psUnit->CharName.c_str(),pUnit->pPlayerData->pClientData->AccountName);
				SendMsgToClient(psUnit->ptClientData, "%s started watching you!", pUnit->pPlayerData->pClientData->AccountName);
				Spec * Data = new Spec;
				Data->ptGame = pUnit->pGame;
				Data->RequesterID = pUnit->dwUnitId;
				Data->SpecID = psUnit->ptPlayer->dwUnitId;

				if (!psUnit->ptPlayer->pPlayerData->isSpecing && !pUnit->pPlayerData->isSpecing)
				{
					_beginthreadex(0, 0, &SpecThread, &Data, 0, 0);
				}
				else
				{
					SendMsgToClient(pUnit->pPlayerData->pClientData, "You're already watching someone!");
				}
				UNLOCK
					return false;
			}
			if (_stricmp(str, "#cheer") == 0)
			{
				if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName)) return TRUE;
				char* Txt = strtok_s(NULL, " ", &t);
				if (!Txt) { SendMsgToClient(pUnit->pPlayerData->pClientData, "#cheer <text> <sound id> <name>");  return false; }
				char* SoundId = strtok_s(NULL, " ", &t);
				if (!SoundId) { SendMsgToClient(pUnit->pPlayerData->pClientData, "#cheer <text> <sound id> <name>");  return false; }
				char* Acc = strtok_s(NULL, " ", &t);
				if (!Acc) { SendMsgToClient(pUnit->pPlayerData->pClientData, "#cheer <text> <sound id> <name>");  return false; }
				WORD sId = (WORD)atoi(SoundId);
				if (sId > 4713) sId = 0;
				ClientData* pDest = FindClientDataByName(pUnit->pGame, Acc);
				if (!pDest) { SendMsgToClient(pUnit->pPlayerData->pClientData, "Player not found!");  return false; }

				SendExEvent(pDest, COL_WHITE, sId, 5, -1, 150, Txt, Txt);
				BroadcastExEvent(pUnit->pGame, COL_WHITE, pDest->pPlayerUnit->dwUnitId, 5, "data\\D2Ex\\Blobs");
				return false;
			}
			if (_stricmp(str, "#1") == 0)
			{
				if (!pUnit->pGame->LastKiller) return true;
				if (pUnit->pGame->LastKiller == pUnit->pPlayerData->pClientData->ClientID) return true;
				ClientData* pDest = FindClientDataById(pUnit->pGame, pUnit->pGame->LastKiller);
				if (!pDest) return true;
				SendExEvent(pDest, COL_WHITE, D2EX_ACHIVEMENT, 3, -1, 150, "#1", "#1");
				BroadcastExEvent(pUnit->pGame, COL_WHITE, pDest->pPlayerUnit->dwUnitId, 2, "data\\D2Ex\\Blobs");
				pUnit->pGame->LastKiller = 0;
				return true;
			}
			if (_stricmp(str, "#0") == 0)
			{
				if (!pUnit->pGame->LastVictim) return true;
				if (pUnit->pGame->LastVictim == pUnit->pPlayerData->pClientData->ClientID) return true;
				ClientData* pDest = FindClientDataById(pUnit->pGame, pUnit->pGame->LastVictim);
				if (!pDest) return true;
				SendExEvent(pDest, COL_PURPLE, D2EX_HUMILATION, 3, -1, 150, "Jestes zerem!", "Such a noob!");
				BroadcastExEvent(pUnit->pGame, COL_PURPLE, pDest->pPlayerUnit->dwUnitId, 2, "data\\D2Ex\\Blobs");
				pUnit->pGame->LastVictim = 0;
				return true;
			}
			if (_stricmp(str, "#roster") == 0)
			{
				LRoster * LR = LRost::Find(pUnit->pGame, pUnit->pPlayerData->szName);
				if (LR)	SendMsgToClient(pUnit->pPlayerData->pClientData, "Kills : %d | Deaths : %d", LR->Kills, LR->Deaths);
				else SendMsgToClient(pUnit->pPlayerData->pClientData, "Kills : 0 | Deaths : 0");
				return false;
			}
			if (_stricmp(str, "#we") == 0)
			{
				if (!wcfgEnableWE) return true;
				int z = NextDC - SellCount; // 261 - 217 ( 44)
				int y = z - MinSell; // 50 -40 =  10
				if (y < 0) y = MinSell + y;  // 40 - 30 = 10
				else y = MinSell;
				if (y == 0) y = 1;
				int x = y + (MaxSell - MinSell);
				SendMsgToClient(pUnit->pPlayerData->pClientData, "World Event is %s, Sell count : %d, Need: %d-%d items", wcfgEnableWE ? "On" : "Off", SellCount, y, x);
				return false;
			}
			if (_stricmp(str, "#uptime") == 0)
			{
				int time = (GetTickCount() - WardenUpTime) / 1000;
				SendMsgToClient(pUnit->pPlayerData->pClientData, "GS Uptime %.2d:%.2d:%.2d, WardenClients %d, pGame->nClients %d", time / 3600, (time / 60) % 60, time % 60, hWarden.Clients.size(), pUnit->pGame->nClients);
				return false;
			}
			if (_stricmp(str, "#build") == 0)
			{
				SendMsgToClient(pUnit->pPlayerData->pClientData, "Program build %d on %s, %s.", __BUILDNO__, __DATE__, __TIME__);
				return false;
			}
			if (_stricmp(str, "#dumpunits") == 0)
			{
				for (int i = 0; i < 128; ++i)
					{
						UnitAny* u = pUnit->pGame->pUnitList[UNIT_OBJECT][i];
						if (u)
						{
							DEBUGMSG("%d:, %d @ [%d,%d]", u->dwType, u->dwClassId, D2Funcs.D2GAME_GetUnitX(u), D2Funcs.D2GAME_GetUnitY(u));
						}
					}
				SendMsgToClient(pUnit->pPlayerData->pClientData, "OK!");
				return false;
			}
			if (_stricmp(str, "#update") == 0)
			{
				if (wcfgUpdateURL.empty()) return false;
				SendMsgToClient(pUnit->pPlayerData->pClientData, "Trying to download patch....");
				ExEventDownload pEvent;
				::memset(&pEvent, 0, sizeof(ExEventDownload));
				pEvent.P_A6 = 0xA6;
				pEvent.MsgType = EXEVENT_DOWNLOAD;
				pEvent.bExec = 1;
				strcpy_s(pEvent.szURL, 255, wcfgUpdateURL.c_str());
				if (pEvent.szURL[0])
					pEvent.PacketLen = 14 + strlen(pEvent.szURL) + 1;
				else
					pEvent.PacketLen = 15;

				D2ASMFuncs::D2GAME_SendPacket(pUnit->pPlayerData->pClientData, (BYTE*)&pEvent, pEvent.PacketLen);
				return false;
			}
			if (_stricmp(str, "#map") == 0)
			{
				if (!pUnit) return TRUE;
				BroadcastMsg(pUnit->pPlayerData->pClientData->pGame, "Map Id : '%d'", pUnit->pPlayerData->pClientData->pGame->InitSeed);
				return true;
			}

			if (_stricmp(str, "#go") == 0 || _stricmp(str, "#g") == 0)
			{
				if (!pUnit->pGame->bFestivalMode) return true;

				int aLevel = D2Funcs.D2COMMON_GetTownLevel(pUnit->dwAct);
				int aCurrLevel = D2Funcs.D2COMMON_GetLevelNoByRoom(pUnit->pPath->pRoom1);
				if (aCurrLevel == aLevel)
				{
					SendMsgToClient(pUnit->pPlayerData->pClientData, pUnit->pPlayerData->pClientData->LocaleID == 10 ? "Najpierw opusc miasto!" : "Leave town first!");
					return true;
				}

				int PartyCount = 0;
				int nPlayers = 0;
				bool InParty = false;

				ostringstream pakiet;

				if (!pUnit->pGame->pPartyControl->ptLastParty)
				{
					for (ClientData* pSingle = pUnit->pGame->pClientList; pSingle; pSingle = pSingle->ptPrevious)
						pakiet << pSingle->CharName << '#' << pSingle->AccountName << '#' << ConvertClass(pSingle->ClassId) << '$';
				}
				else
					for (SubParty* pParty = pUnit->pGame->pPartyControl->ptLastParty; pParty; pParty = pParty->ptPrev)
					{
						for (PartyMembers* pMem = pParty->ptMembers; pMem; pMem = pMem->ptNext)
						{
							if (pMem->dwUnitId == pUnit->dwUnitId)  InParty = true;

							UnitAny* pMate = D2ASMFuncs::D2GAME_FindUnit(pUnit->pGame, pMem->dwUnitId, UNIT_PLAYER);
							if (pMate) {
								pakiet << pMate->pPlayerData->szName << '#' << pMate->pPlayerData->pClientData->AccountName << '#' << ConvertClass(pMate->dwClassId) << ':';
							}
							nPlayers++;
						}
						if (pParty->ptPrev) pakiet << '$';
						PartyCount++;
					}


				if (pUnit->pPlayerData->SaidGO) {
					SendMsgToClient(pUnit->pPlayerData->pClientData, pUnit->pPlayerData->pClientData->LocaleID == 10 ? "Powiedzia�e� ju� to!" : "You've already said that", PartyCount);
					return false;
				}

				if (PartyCount == 0 && pUnit->pPlayerData->SaidGO == 0)
				{
					pUnit->pPlayerData->SaidGO = 1;
					pUnit->pPlayerData->isPlaying = 1;
					pUnit->pGame->nPlaying++;

					if (pUnit->pGame->nPlaying > 1)
					{
						ClearSayGoFlag(pUnit->pGame);

						for (ClientData* i = pUnit->pGame->pClientList; i; i = i->ptPrevious)
						{
							if (!i->pGame) continue;
							if (i->InitStatus != 4) continue;
							if (i->pGame == pUnit->pGame && i->pPlayerUnit->pPlayerData->isPlaying != 0)  i->pPlayerUnit->pPlayerData->CanAttack = 1;
						}
						BroadcastExEvent(pUnit->pGame, COL_RED, D2EX_PLAY, 3, -1, 200, "Runda rozpoczeta!", "Round begins!");
					}
					return true;
				}

				if (PartyCount == 2 && pUnit->pPlayerData->SaidGO == 0 && InParty)
				{
					pUnit->pPlayerData->SaidGO = 1;
					pUnit->pPlayerData->isPlaying = 1;
					pUnit->pGame->nPlaying++;

					if (pUnit->pGame->nPlaying >= nPlayers) {
						ClearSayGoFlag(pUnit->pGame);

						for (ClientData* i = pUnit->pGame->pClientList; i; i = i->ptPrevious)
						{
							if (!i->pGame) continue;
							if (i->InitStatus != 4) continue;
							if (i->pGame == pUnit->pGame && i->pPlayerUnit->pPlayerData->isPlaying != 0) i->pPlayerUnit->pPlayerData->CanAttack = 1;

						}

						BroadcastExEvent(pUnit->pGame, 1, D2EX_PLAY, 3, -1, 200, "Runda rozpoczeta!", "Round begins!");
					}
					return true;
				}
				SendMsgToClient(pUnit->pPlayerData->pClientData, pUnit->pPlayerData->pClientData->LocaleID == 10 ? "Nieprawidlowa ilosc druzyn - %d " : "Incorrect party count - %d", PartyCount);

				return true;
			}
			if (_stricmp(str, "#gu") == 0)
			{
				if (wcfgAllowGU)
				{
					int aLevel = D2Funcs.D2COMMON_GetTownLevel(pUnit->dwAct);
					int aCurrLevel = D2Funcs.D2COMMON_GetLevelNoByRoom(pUnit->pPath->pRoom1);
					if (aCurrLevel != aLevel)
						D2ASMFuncs::D2GAME_MoveUnitToLevelId(pUnit, aLevel, pUnit->pGame);
				}
				if (pUnit->pGame->bFestivalMode == 1)
					if (pUnit->pPlayerData->isPlaying) {
						pUnit->pPlayerData->SaidGO = 0;
						pUnit->pPlayerData->CanAttack = 0;
						DoRoundEndStuff(pUnit->pGame, pUnit);
						pUnit->pPlayerData->isPlaying = 0;
						pUnit->pGame->nPlaying--;
					}

				return TRUE;
			}
			if (_stricmp(str, "#debug") == 0)
			{
				WardenClient_i ptCurrentClient = GetClientByID(ClientID);
				if (ptCurrentClient == hWarden.Clients.end()) return TRUE;
				ptCurrentClient->DebugTrick = !ptCurrentClient->DebugTrick;
				UNLOCK
					return false;
			}
			if (_stricmp(str, "#reload") == 0)
			{
				if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName)) return TRUE;
				Warden_Config();
				SendMsgToClient(pUnit->pPlayerData->pClientData, pUnit->pPlayerData->pClientData->LocaleID == 10 ? "Ustawienia prze�adowane." : "Config reloaded.");
				return false;
			}
			if (_stricmp(str, "#kick") == 0)
			{
				if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName)) return TRUE;

				str = strtok_s(NULL, " ", &t);
				if (!str) { SendMsgToClient(pUnit->pPlayerData->pClientData, "#kick <*account> or #kick [charname]!"); return false; }
				WardenClient_i psUnit = hWarden.Clients.end();
				if (str[0] == '*') {
					str++;
					psUnit = GetClientByAcc(str);
				}
				else
					psUnit = GetClientByName(str);

				if (psUnit == hWarden.Clients.end()) { SendMsgToClient(pUnit->pPlayerData->pClientData, "Wrong charname / Player is not in the game!"); return false; }
				BroadcastMsg(pUnit->pPlayerData->pClientData->pGame, "'%s' has been kicked by *%s", psUnit->CharName.c_str(), pUnit->pPlayerData->pClientData->AccountName);
				KickPlayer(psUnit->ptClientData);
				UNLOCK
					return false;
			}
			if (_stricmp(str, "#stats") == 0)
			{
				UnitAny * pDestUnit = pUnit;

				str = strtok_s(NULL, " ", &t);
				if (str)
				{
					if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName)) return TRUE;

					WardenClient_i ptCurrentClient = hWarden.Clients.end();
					if (str[0] == '*') {
						str++;
						ptCurrentClient = GetClientByAcc(str);
					}
					else
						ptCurrentClient = GetClientByName(str);

					if (ptCurrentClient == hWarden.Clients.end()) { SendMsgToClient(pUnit->pPlayerData->pClientData, "Player not found!"); return false; }
					pDestUnit = ptCurrentClient->ptPlayer;
					UNLOCK
				}

				if (!pDestUnit) return false;

				int sFR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_FIRERESIST, 0);
				int sCR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_COLDRESIST, 0);
				int sLR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_LIGHTNINGRESIST, 0);
				int sPR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_POISONRESIST, 0);

				int sDR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_DAMAGEREDUCTION, 0);

				int sFCR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_FASTERCAST, 0);
				int sFHR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_FASTERHITRECOVERY, 0);
				int sIAS = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_IAS, 0);
				int sFRW = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_FASTERRUNWALK, 0);
				int sDS = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_DEADLYSTRIKE, 0);
				int sOW = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_OPENWOUNDS, 0);
				int sCB = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_CRUSHINGBLOW, 0);

				int sCABS = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_COLDABSORBPERCENT, 0);
				int sLABS = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_LIGHTNINGABSORBPERCENT, 0);
				int sFABS = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_FIREABSORBPERCENT, 0);

				int sMFR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_MAXFIRERESIST, 0) + 75;
				int sMCR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_MAXCOLDRESIST, 0) + 75;
				int sMLR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_MAXLIGHTNINGRESIST, 0) + 75;
				int sMPR = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_MAXPOISONRESIST, 0) + 75;

				int sMF = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_MAGICFIND, 0);
				int aLife = D2Funcs.D2COMMON_GetUnitMaxLife(pDestUnit) >> 8;
				int aMana = D2Funcs.D2COMMON_GetUnitMaxMana(pDestUnit) >> 8;

				int sREP = D2Funcs.D2COMMON_GetStatSigned(pDestUnit, STAT_HPREGEN, 0);

				if (D2Funcs.D2COMMON_GetUnitState(pDestUnit, 83))  //Nat res
				{
					Skill* pSkill = D2Funcs.D2COMMON_GetSkillById(pDestUnit, 153, -1);
					if (pSkill)
					{
						int SkillLvl = D2Funcs.D2COMMON_GetSkillLevel(pDestUnit, pSkill, 1);
						int ResBonus = D2Funcs.D2COMMON_EvaluateSkill(pDestUnit, 3442, 153, SkillLvl);
						sFR -= ResBonus;
						sCR -= ResBonus;
						sLR -= ResBonus;
						sPR -= ResBonus;
					}
				}
				if (D2Funcs.D2COMMON_GetUnitState(pDestUnit, 8)) //Salv
				{
					StatList * Stats = D2Funcs.D2COMMON_GetStateStatList(pDestUnit, 8);
					int ResBonus = Stats->Stats.pStat->dwStatValue;
					sFR -= ResBonus;
					sCR -= ResBonus;
					sLR -= ResBonus;
				}

				SendMsgToClient(pUnit->pPlayerData->pClientData, "Resistances : �c1Fire '%d'/'%d%%', �c9Light '%d'/'%d%%', �c3Cold '%d'/'%d%%', �c2Poison '%d'/'%d%%'", sFR, sMFR, sLR, sMLR, sCR, sMCR, sPR, sMPR);
				SendMsgToClient(pUnit->pPlayerData->pClientData, "FCR �c9'%d'�cc, FRW �c9'%d'�cc, FHR �c9'%d'�cc, IAS �c9'%d'", sFCR, sFRW, sFHR, sIAS);
				SendMsgToClient(pUnit->pPlayerData->pClientData, "DS �c9'%d%%'�cc, OW �c9'%d%%'�cc, CB �c9'%d%%'�cc, DR �c9'%d%%'", sDS, sOW, sCB, sDR);
				SendMsgToClient(pUnit->pPlayerData->pClientData, "�c1Fire Abs �c9'%d%%' �c3Cold Abs �c9'%d%%' �c9Light Abs �c9'%d%%'�c;, MF �c9'%d%%'", sFABS, sCABS, sLABS, sMF);
				SendMsgToClient(pUnit->pPlayerData->pClientData, "�c1Life : '%d', �c3Mana : '%d', �ccLife Replenish : �c9'%d'", aLife, aMana, sREP);
				return false;
			}
			if (_stricmp(str, "#move") == 0)
			{
				UnitAny* aUnit = pUnit;
				if (!isAnAdmin(pUnit->pPlayerData->pClientData->AccountName))  return TRUE;
				str = strtok_s(NULL, " ", &t);
				if (!str) { SendMsgToClient(pUnit->pPlayerData->pClientData, "#move <levelid> [account]"); return false; }
				int LvlId = atoi(str);
				str = strtok_s(NULL, " ", &t);
				if (str) {
					WardenClient_i ptCurrentClient = GetClientByName(str);
					if (ptCurrentClient == hWarden.Clients.end()) { SendMsgToClient(pUnit->pPlayerData->pClientData, "Player not found!"); return false; }
					if (!ptCurrentClient->ptPlayer) { UNLOCK return false; }
					if (aUnit == ptCurrentClient->ptPlayer){ UNLOCK return false; }
					aUnit = ptCurrentClient->ptPlayer;
					UNLOCK
				}
				if (!LvlId) return false;
				if (LvlId > 136) return false;
				SendMsgToClient(aUnit->pPlayerData->pClientData, "Moving '%s' to level '%d'...", aUnit->pPlayerData->szName, LvlId);
				D2ASMFuncs::D2GAME_MoveUnitToLevelId(aUnit, LvlId, aUnit->pGame);
				return false;
			}

		}
	}
	return TRUE;
}