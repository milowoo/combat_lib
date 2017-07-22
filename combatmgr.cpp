/******************************************************
 *			Copyright 2013, by linglong.
 *				All right reserved.
 *功能：战斗处理模块
 *日期：2014-2-10 20:15
 *作者：milo_woo
 ******************************************************/
#include<math.h>
#include "combatmgr.h"
#include "module.h"

CCombatMgr::CCombatMgr()
{
     m_nMaxFightCount = 25;
	 m_mapProfLevel.clear();
}

CCombatMgr::~CCombatMgr()
{

}

void CCombatMgr::relolutionTable(bool bUpdate)
{
	if (g_pConsummer == NULL)
	{
		return ;
	}

	m_mapProfLevel.clear();
	CTableDataPtr ptabledata = g_pConsummer->getStaticData("tb_table_profession_restrain", bUpdate);
	if (ptabledata != NULL)
	{
		for (Json::Value::iterator iter=ptabledata->m_Value.begin(); iter!=ptabledata->m_Value.end(); ++iter)
		{
			Json::Value& tmpvalue = (*iter);	
			CProfLevel cInf;
			cInf.m_nInitRestVal = tmpvalue["init_restrain_val"].asInt();
			cInf.m_nGrowRestVal = tmpvalue["grow_restrain_val"].asInt();
			uint32 profession = tmpvalue["profession"].asInt();
			uint32 resprof = tmpvalue["restrain_prof"].asInt();
			uint32 nKeyVal = profession * 100 + resprof;
			m_mapProfLevel.insert(make_pair(nKeyVal, cInf));
		}
	}
}

/*
*@战斗过程处理
* mapAvatarPtr 参与对象
* gwresponse 战斗数据
*/
void  CCombatMgr::combatProc(std::map<OBJID, CFightObjPtr> &mapAvatarPtr, CFightData &cFightData, uint8 nCalStar/*=1*/)
{
	//初始化阵型信息
	uint32 attackSpeed = 0;
	uint32 attackHp = 0;
	this->formationObjInit(cFightData.m_vecFormat, mapAvatarPtr, attackSpeed, attackHp, cFightData.m_nType);
	uint32 blowSpeed = 0;
    uint32 blowHp = 0;
	this->formationObjInit(cFightData.m_vecRevFormat, mapAvatarPtr, blowSpeed, blowHp, cFightData.m_nType);

	//获取每次战斗的次数
    size_t maxfightnum =  0;
	if (cFightData.m_vecRevFormat.size() > cFightData.m_vecFormat.size())
		maxfightnum = cFightData.m_vecRevFormat.size();
	else
		maxfightnum = cFightData.m_vecFormat.size();

	//最多循环25回合
	size_t fight_count = 0;
	bool bfalive = true;
	bool bralice = true;
	for ( ; fight_count < m_nMaxFightCount; fight_count++)
	{
		//判断挑战方是否还有对象存活
		bfalive = this->isHavaAlive(cFightData.m_vecFormat, mapAvatarPtr);
		bralice = this->isHavaAlive(cFightData.m_vecRevFormat, mapAvatarPtr);
		if (!bfalive)
		{
			cFightData.m_nFightRet = 1; //战斗结果 0挑战成功 1挑战失败 2挑战平局
			break;
		}

		if (!bralice)
		{
			cFightData.m_nFightRet = 0; //战斗结果 0挑战成功 1挑战失败 2挑战平局
			break;
		}

		//重置阵型上对象的出手标志
		this->resetFightFlag(mapAvatarPtr);

		//循环一轮处理
		CFightObjPtr cAvatarFPtr = NULL;
		CFightObjPtr cAvatarRPtr = NULL;
		for (size_t i = 0; i <= maxfightnum; i++)
		{
			CAttackRound cAttackRound;
			cAttackRound.m_nCurRound = (uint8)fight_count;
			//挑战方第一个为出手者
		    cAvatarFPtr = this->getFirstNoFightAvatar(cAttackRound.m_nCurRound, cFightData.m_vecFormat, mapAvatarPtr);

			//受战方第一个出手者
			cAvatarRPtr = this->getFirstNoFightAvatar(cAttackRound.m_nCurRound, cFightData.m_vecRevFormat, mapAvatarPtr);

			//双发都没有出手者 -- 本轮战斗结束
			if (cAvatarFPtr == NULL && cAvatarRPtr == NULL)
				break;

			uint8 dragonflag = 0;  //0没有灵龙1单方灵龙2双方都有灵龙

            //只有挑战方有未出手者
			if (cAvatarFPtr != NULL && cAvatarRPtr == NULL)
			{
				//判断受战方是否有存活者
				bralice = this->isHavaAlive(cFightData.m_vecRevFormat, mapAvatarPtr);
				if (bralice)
				{
					this->getAttackRound(cAvatarFPtr, cFightData.m_vecFormat, cFightData.m_vecRevFormat, mapAvatarPtr, cAttackRound);
					if (cAttackRound.m_nFightID > 0)
					    cFightData.m_vecAttackRound.push_back(cAttackRound);
				}
			}
			else if (cAvatarFPtr == NULL && cAvatarRPtr != NULL) //只有受战方有未出手者 
			{
				//判断挑战方是否有存活者
				bfalive = this->isHavaAlive(cFightData.m_vecFormat, mapAvatarPtr);
				if (bfalive)
				{
					this->getAttackRound(cAvatarRPtr, cFightData.m_vecRevFormat, cFightData.m_vecFormat, mapAvatarPtr, cAttackRound);
					if (cAttackRound.m_nFightID > 0)
					    cFightData.m_vecAttackRound.push_back(cAttackRound);
				}
			}
			else //双方都有未出手的
			{
				uint8 fightfirst = 0; //0挑战方先出手 1防守方先出手
				//判断双方的速度 有灵龙的处理
				if (cAvatarFPtr->getAvatarType() == AVATAR_TYPE_DRAGON || cAvatarRPtr->getAvatarType() == AVATAR_TYPE_DRAGON)
				{
					dragonflag = 1;
					if (cAvatarFPtr->getAvatarType() == AVATAR_TYPE_DRAGON && cAvatarRPtr->getAvatarType() == AVATAR_TYPE_DRAGON)
					{
						dragonflag = 2;
						if (attackSpeed >= blowSpeed)
						{
							this->getAttackRound(cAvatarFPtr, cFightData.m_vecFormat, cFightData.m_vecRevFormat, mapAvatarPtr, cAttackRound);
						}
						else
						{
							fightfirst = 1;
							this->getAttackRound(cAvatarRPtr, cFightData.m_vecRevFormat, cFightData.m_vecFormat, mapAvatarPtr, cAttackRound);
						}
					}
					else if (cAvatarFPtr->getAvatarType() == AVATAR_TYPE_DRAGON)
					{
						this->getAttackRound(cAvatarFPtr, cFightData.m_vecFormat, cFightData.m_vecRevFormat, mapAvatarPtr, cAttackRound);
					}
					else
					{
						fightfirst = 1;
						this->getAttackRound(cAvatarRPtr, cFightData.m_vecRevFormat, cFightData.m_vecFormat, mapAvatarPtr, cAttackRound);
					}
				}
				else
				{
					//判断双方的速度
					if (cAvatarFPtr->getAttr(ATTR_Speed) > cAvatarRPtr->getAttr(ATTR_Speed))
					{
						this->getAttackRound(cAvatarFPtr, cFightData.m_vecFormat, cFightData.m_vecRevFormat, mapAvatarPtr, cAttackRound);
					}
					else if (cAvatarFPtr->getAttr(ATTR_Speed) < cAvatarRPtr->getAttr(ATTR_Speed))
					{
						fightfirst = 1;
						this->getAttackRound(cAvatarRPtr, cFightData.m_vecRevFormat, cFightData.m_vecFormat, mapAvatarPtr, cAttackRound);
					}
					else
					{
						//速度相等， 50%的概率
						uint32 radomvalues = CRandom::rand(10000);
						if (radomvalues <= 5000)
						{
							this->getAttackRound(cAvatarFPtr, cFightData.m_vecFormat, cFightData.m_vecRevFormat, mapAvatarPtr, cAttackRound);
						}
						else
						{
							fightfirst = 1;
							this->getAttackRound(cAvatarRPtr, cFightData.m_vecRevFormat, cFightData.m_vecFormat, mapAvatarPtr, cAttackRound);
						}
					}
				}

				//记录出手战斗数据
				if (cAttackRound.m_nFightID > 0)
				    cFightData.m_vecAttackRound.push_back(cAttackRound);

                //如果是该回合第一次出手，并且不都是灵龙
			    if (i == 0 && dragonflag == 1)
					continue;

				CAttackRound cRevRound;
				cRevRound.m_nCurRound = (uint8)fight_count;

				//判断是否还有人存活
				if (!this->isHavaAlive(cFightData.m_vecFormat, mapAvatarPtr))
					break;

				if (!this->isHavaAlive(cFightData.m_vecRevFormat, mapAvatarPtr))
					break;
                
				//如果是挑战方先出手
				if (fightfirst == 0)
				{
					cAvatarRPtr = this->getFirstNoFightAvatar(cAttackRound.m_nCurRound, cFightData.m_vecRevFormat, mapAvatarPtr);
					if (cAvatarRPtr != NULL)
					{
						this->getAttackRound(cAvatarRPtr, cFightData.m_vecRevFormat, cFightData.m_vecFormat, mapAvatarPtr, cRevRound);
					}
				}
				else  //防守方先出手
				{
					cAvatarFPtr = this->getFirstNoFightAvatar(cAttackRound.m_nCurRound, cFightData.m_vecFormat, mapAvatarPtr);
					if (cAvatarFPtr != NULL)
					{
						this->getAttackRound(cAvatarFPtr, cFightData.m_vecFormat, cFightData.m_vecRevFormat, mapAvatarPtr, cRevRound);
					}
				}
                
				//记录回击战斗数据
				if (cRevRound.m_nFightID > 0)
				    cFightData.m_vecAttackRound.push_back(cRevRound);
			}
		}
	}
	this->avatarOutFight(cFightData.m_vecFormat, mapAvatarPtr);
	this->avatarOutFight(cFightData.m_vecRevFormat, mapAvatarPtr);
 
	if (fight_count >= 25)
		cFightData.m_nFightRet = 2; //战斗结果 0挑战成功 1挑战失败 2挑战平局	
    
	if (nCalStar)
	{
		//计算星级
		if (cFightData.m_nFightRet == 0)
			cFightData.m_nStar = this->getSuccessStar(cFightData.m_vecFormat, mapAvatarPtr, attackHp);
		else
			cFightData.m_nStar = 0;
	}
    
	//攻击方剩余血量
	uint32 attackSurplusHp = this->formationSurplusHp(cFightData.m_vecFormat, mapAvatarPtr);
	//防守方剩余血量
	uint32 blowSurPlusHp = this->formationSurplusHp(cFightData.m_vecRevFormat, mapAvatarPtr);
    
	//双方都阵亡的特殊情况
	if (attackSurplusHp == 0 && blowSurPlusHp == 0)
		cFightData.m_nFightRet = 2;

	cFightData.m_nAttackBlood = static_cast<int32>(attackHp - attackSurplusHp);//(attackHp >= attackSurplusHp) ? (attackHp - attackSurplusHp) : attackHp;
	cFightData.m_nBlowBlood = static_cast<int32>(blowHp - blowSurPlusHp);//(blowHp >= blowSurPlusHp) ? (blowHp - blowSurPlusHp) : blowHp;
	cFightData.m_nFightCount = fight_count;
	return;
}

/*
*@成功挑战星级
*/
uint8 CCombatMgr::getSuccessStar(std::vector<CFormatUint> &vecFormat,  std::map<OBJID, CFightObjPtr> &mapBlowObj, uint32 totHp)
{
	uint32 lastHp = 0;
	bool allalive = true;
	for (size_t i = 0; i < vecFormat.size(); i++)
	{
		CFormatUint &cUint = vecFormat[i];
		if (cUint.m_nGrid == 16)
			continue;
		std::map<OBJID, CFightObjPtr>::iterator iter = mapBlowObj.find(cUint.m_nFightId);
		if (iter != mapBlowObj.end())
		{
			lastHp += iter->second->getHp();

			if (iter->second->isDie())
				allalive = false;
		}
	}

	uint32 lastrate = lastHp * 100 / totHp;
	if (lastrate > 80 && allalive)
		return 5;
	if (lastrate > 60)
		return 4;
	if (lastrate > 40)
		return 3;
	if (lastrate > 20)
		return 2;
	return 1;
}

/*
*@失败挑战星级
*/
uint8 CCombatMgr::getFailStar(std::vector<CFormatUint> &vecFormat,  std::map<OBJID, CFightObjPtr> &mapBlowObj, uint32 totHp)
{
	uint32 lastHp = 0;
	for (size_t i = 0; i < vecFormat.size(); i++)
	{
		CFormatUint &cUint = vecFormat[i];
		if (cUint.m_nGrid == 16)
			continue;
		std::map<OBJID, CFightObjPtr>::iterator iter = mapBlowObj.find(cUint.m_nFightId);
		if (iter != mapBlowObj.end())
		{
			lastHp += iter->second->getHp();
		}
	}

	uint32 lastrate = lastHp * 100 / totHp;
	if (lastrate > 20)
		return 2;
	return 1;
}

/*
*@校验阵型是否还有对象存活
*/
bool CCombatMgr::isHavaAlive(std::vector<CFormatUint> &vecFormat, std::map<OBJID, CFightObjPtr> &mapBlowObj)
{
	for (size_t i = 0; i < vecFormat.size(); i++)
	{
		
		CFormatUint &cUint = vecFormat[i];
		if (cUint.m_nGrid == 16)
			continue;

		OBJID idAvatar = cUint.m_nFightId;
		std::map<OBJID, CFightObjPtr>::iterator iter = mapBlowObj.find(idAvatar);
		if (iter != mapBlowObj.end())
		{
			CFightObjPtr cAvatarPtr= iter->second;
			if (cAvatarPtr != NULL)
			{
			   if (!cAvatarPtr->isDie())
				   return true;
			}
		}
	}

	return false;
}

/*
*@重置出手标志
*/
void CCombatMgr::resetFightFlag(std::map<OBJID, CFightObjPtr> &mapBlowObj)
{
	std::map<OBJID, CFightObjPtr>::iterator iter = mapBlowObj.begin();
	for (; iter != mapBlowObj.end(); ++iter)
	{
		CFightObjPtr cAvatarPtr= iter->second;
		if (cAvatarPtr != NULL)
				cAvatarPtr->setFightFlag(false);
	}
}

/*
*@找出第一个未出手者
*/
CFightObjPtr CCombatMgr::getFirstNoFightAvatar(uint8 nRound,std::vector<CFormatUint> &vecFormat, std::map<OBJID, CFightObjPtr> &mapBlowObj)
{
	for (size_t i = 0; i < vecFormat.size(); i++)
	{
		CFormatUint &cUint = vecFormat[i];
		OBJID idAvatar = cUint.m_nFightId;
		std::map<OBJID, CFightObjPtr>::iterator iter = mapBlowObj.find(idAvatar);
		if (iter != mapBlowObj.end())
		{
			CFightObjPtr cAvatarPtr = iter->second;
			if (cAvatarPtr != NULL)
			{	
				//判断是否是灵龙 
				if (cAvatarPtr->getAvatarType() == AVATAR_TYPE_DRAGON)
				{
					//判断回合数是否大于灵龙的技能
					std::vector<CLSkillData> vecUseSkill;
					cAvatarPtr->getUseSkill(vecUseSkill);
					if (vecUseSkill.size() > 0)
					{
						if (nRound <= vecUseSkill.size() - 1 && 
							(!cAvatarPtr->getFightFlag()) )
							return cAvatarPtr;
					}
				}
				else
				{
					if ((!cAvatarPtr->isDie()) && 
						(!cAvatarPtr->getFightFlag()))
						return cAvatarPtr;
				}
				
			}
		}
	}

	return NULL;
}

/*
*@获取灵龙战斗技能
* 
*/
void CCombatMgr::getDragonFightSkill(uint8 nRound, std::vector<CLSkillData> &vecSkill, CLSkillData &cSkill)
{
	if (nRound >= vecSkill.size())
		return;
	cSkill.m_nSkillID = vecSkill[nRound].m_nSkillID;
	cSkill.m_nSKillLevel = vecSkill[nRound].m_nSKillLevel;
	cSkill.m_nExp = vecSkill[nRound].m_nExp;

	uint32 level = cSkill.m_nSkillID%100;

	if (cSkill.m_nSKillLevel > 1)
	    cSkill.m_nSkillID = cSkill.m_nSkillID + cSkill.m_nSKillLevel - 1;

	if (level == 0)
		cSkill.m_nSkillID++;

	return;
}

/*
*@出战单位一次攻击处理
*cAvatarPtr 出手者 mapBlowObj 全体出战对象 vecFormat 出手方阵型 vecRecFormat 受击方阵型
*gwresponse 战斗数据
*/
void CCombatMgr::getAttackRound(CFightObjPtr cAvatarPtr, std::vector<CFormatUint> &vecFormat, std::vector<CFormatUint> &vecRecFormat,
								  std::map<OBJID, CFightObjPtr> &mapBlowObj, CAttackRound &cAttackRound)
{

	OBJID idAvatar = cAvatarPtr->getAvatarID();

	//设置出手标志
	cAvatarPtr->setFightFlag(true);

	//出手者
	cAttackRound.m_nFightID = cAvatarPtr->getAvatarID();

	//出手者BUFF处理 
	uint8 buffret = 0;
	CLSkillData cSkill;
	if (cAvatarPtr->getAvatarType() != AVATAR_TYPE_DRAGON) //灵龙不需要判断BUFF 
	{
		//删除轮回完成的BUFF
		std::vector<uint32> vecDelBuff;
		cAvatarPtr->getInvalidBuff(vecDelBuff);
		if (vecDelBuff.size() > 0)
		{
			cAttackRound.m_vecDelBuff.assign(vecDelBuff.begin(), vecDelBuff.end());
			for (size_t i = 0; i < vecDelBuff.size(); i++)
				cAvatarPtr->delBuff(vecDelBuff[i]);

			//出手者BUFF处理
			cAvatarPtr->updateAttr(ATTRTYPE_BUFF, true);
		}

		buffret = buffEffectProc(cAvatarPtr, cAttackRound.m_vecBuffEff);
		if (buffret == BUFF_PROC_END_VERTIGO) //有眩晕的BUFF
		{
			//所有BUFF回合数都减一
			cAvatarPtr->SenceBuffDel();
            //删除轮回完成的BUFF
			std::vector<uint32> vecDelBuff;
			cAvatarPtr->getInvalidDOTBuff(vecDelBuff);
			if (vecDelBuff.size() > 0)
			{
				for (size_t i = 0; i < vecDelBuff.size(); i++)
				{
					cAttackRound.m_vecDelBuff.push_back(vecDelBuff[i]);
					cAvatarPtr->delBuff(vecDelBuff[i]);
				}		
			}
			return;
		}
        
		//处理天赋技能
		if (cAvatarPtr->getAvatarType() == AVATAR_TYPE_BUDDY)
			cAvatarPtr->updateFixBuffAttr(true, false);

		//如果有沉默BUFF,则选择普通技能
		if (buffret == BUFF_PROC_END_SILENCE || 0 == cAvatarPtr->getEn()
			|| cAvatarPtr->getEn() < cAvatarPtr->getMaxEn())
			cAvatarPtr->getNSkill(cSkill);
		else
			cAvatarPtr->getESkill(cSkill);
	}
	else
	{
		//获取灵龙的战斗技能
		std::vector<CLSkillData> vecSkill;
		cAvatarPtr->getUseSkill(vecSkill);

		this->getDragonFightSkill(cAttackRound.m_nCurRound, vecSkill,cSkill);
	}
    //如果攻击者阵亡，则不需要继续处理
	if (cAvatarPtr->isDie() && cAvatarPtr->getAvatarType() != AVATAR_TYPE_DRAGON)
		return;
    
	//获取技能信息
    std::vector<uint32> vecEffectID;
	g_pConsummer->getSkillEffect(cSkill.m_nSkillID, vecEffectID);
    
	//使用的技能
	cAttackRound.m_nSkillID = cSkill.m_nSkillID;

	const Json::Value& skillConf = g_pConsummer->skill(cAttackRound.m_nSkillID);
	if (skillConf == Json::nullValue)
	{
		//所有BUFF回合数都减一
		cAvatarPtr->SenceBuffDel();
		return;
	}

	//是否有反击的数据
	uint8 skill_obj_choice = skillConf["skill_obj_choice"].asUInt(); //技能选择目标
	uint8 skill_grid = 127;   //技能选择的上次目标格子

    //先消耗出手者的怒气
	cAttackRound.m_nEn = skillConf["use_en"].asInt() * (-1);
	cAvatarPtr->changeEn(cAttackRound.m_nEn);

	//循环处理技能的效果
	for (size_t i = 0; i < vecEffectID.size(); i++)
	{
		uint32 idEffect = vecEffectID[i];

		//追击类技能id
		std::vector<uint32> vecChaseEffectId;
		vecChaseEffectId.clear();

		CAttackWave cWave;
		bool bresult = this->skillEffectProc(cAvatarPtr, cSkill.m_nSkillID, idEffect, vecFormat, vecRecFormat, mapBlowObj, 
			                                 skill_obj_choice, skill_grid, cWave, vecChaseEffectId);
		if (!bresult)
			continue;

		cAttackRound.m_vecAttackWave.push_back(cWave);

		if( vecChaseEffectId.size() != 0 ) //触发追击类效果
		{
			//获取技能信息
			for( size_t j = 0; j < vecChaseEffectId.size(); j++ )
				vecEffectID.push_back(vecChaseEffectId[j]);
		}

        //如果攻击者死亡， 则不需要继续处理
		if (cAvatarPtr->isDie() && cAvatarPtr->getAvatarType() != AVATAR_TYPE_DRAGON)
			return;
	}

	//所有BUFF回合数都减一
	cAvatarPtr->SenceBuffDel();
    //灵龙不需要判断BUFF 
	if (cAvatarPtr->getAvatarType() != AVATAR_TYPE_DRAGON) 
	{
		//删除DOT类的BUFF
		std::vector<uint32> vecDelBuff;
		cAvatarPtr->getInvalidDOTBuff(vecDelBuff);
		if (vecDelBuff.size() > 0)
		{
			for (size_t i = 0; i < vecDelBuff.size(); i++)
			{
				cAttackRound.m_vecDelBuff.push_back(vecDelBuff[i]);
				cAvatarPtr->delBuff(vecDelBuff[i]);
			}		
		}
	}
	return;
}


/*
*@出手者BUFF处理
*/
uint8 CCombatMgr::buffEffectProc(CFightObjPtr cAvatarPtr, std::vector<CBuffEffect> &vecBuffEffect)
{
	uint8 ret = BUFF_PROC_END_CONTINUE;
	//判断出手者是否有BUFF
	std::vector<CBuffInfoEx> vecBuff;
	cAvatarPtr->getSenceBuff(vecBuff);
	if (vecBuff.size() == 0)
		return ret;

	//找出DOT的BUFF,进行伤害处理
	std::vector<uint32> vecDotBuff;
	std::vector<std::string> vecstr;
	int32 chghp = 0; //血量变化
	for (size_t i = 0; i < vecBuff.size(); i++)
	{
		CBuffInfoEx &cInf = vecBuff[i];
		uint32 idBuff = cInf.m_nBuffId;

		uint32 buffeffect = cInf.m_nBuffEffect;

		if (IS_CONTROL_BUFF_EFFECT(buffeffect))
		{
			if (buffeffect == CONTROL_BUFF_EFFECT_VERTIGO || buffeffect == CONTROL_BUFF_EFFECT_FORCE_VERTIGO)
				ret = BUFF_PROC_END_VERTIGO;
			else if (buffeffect == CONTROL_BUFF_EFFECT_SILENCE || buffeffect == CONTROL_BUFF_EFFECT_FORCE_SILENCE)
			{
				if (ret == BUFF_PROC_END_CONTINUE) 
					ret = BUFF_PROC_END_SILENCE;
			}

			continue;
		}

		if ((!IS_DOT_BUFF_EFFECT(buffeffect)) && \
			(!IS_POISON_NO_HP_ADD_EFFECT(buffeffect)))  //如果不是DOT类并且不是DOT带禁疗BUFF。则不需要处理
			continue;

		if (cAvatarPtr->isHaveDOTImmBuff())
			continue;

		std::string  streffectvalues = cInf.m_strBuffVal;

		vecDotBuff.push_back(idBuff);
		switch (buffeffect)
		{
		case DOT_BUFF_EFFECT_POISON_ATTACK: //中毒（攻击*系数）
		case POISON_NO_HP_ADD_EFFECT_ATTACK://中毒（攻击*系数）带禁疗
		case DOT_BUFF_EFFECT_BLOOD_ATTACK: //流血（攻击*系数）
		case DOT_BUFF_EFFECT_LIGHT_ATTACK: //点燃（攻击*系数）
			{
				uint32 effectvalues  = atoi(streffectvalues.data());
				uint32 hp = cInf.m_nAttackValues * effectvalues / 10000;
				chghp += hp;
			}
			break;
		case DOT_BUFF_EFFECT_POISON_RATE: //中毒（百分比）
		case POISON_NO_HP_ADD_EFFECT_RATE://中毒（百分比）带禁疗
		case DOT_BUFF_EFFECT_BLOOD_RATE: // 流血（百分比）
		case DOT_BUFF_EFFECT_LIGHT_RATE: //点燃（百分比）
			{
				uint32 effectvalues  = atoi(streffectvalues.data());
				uint32 hp = cAvatarPtr->getMaxHP() * effectvalues /10000;
				chghp += hp;
			}
			break;
		case DOT_BUFF_EFFECT_POISON_HP: //中毒（兵力上限*x + y）
		case POISON_NO_HP_ADD_EFFECT_HP://中毒（兵力上限*x + y）带禁疗
		case DOT_BUFF_EFFECT_BLOOD_HP: // 流血（兵力上限*x + y）
		case DOT_BUFF_EFFECT_LIGHT_HP: //点燃（兵力上限*x + y）
			{
				double dX = 0.0;
				uint32 nY = 0;
				if( 2 != sscanf(streffectvalues.c_str(), "%lf:%d", &dX, &nY) )
					break;

				uint32 nrate = uint32(dX * 10000);
				uint32 hp = cAvatarPtr->getMaxHP() * nrate / 10000 + nY;
				chghp += hp;
			}
			break;
		default:
			break;
		}
	}

	if (vecDotBuff.size() > 0)
	{
		CBuffEffect  cBuffEffect;
		cBuffEffect.m_vecUseBuff.assign(vecDotBuff.begin(), vecDotBuff.end());
		int32 finhp = cAvatarPtr->getFinalHurt(chghp);
		if (finhp > 0 && !cAvatarPtr->isHaveDevineShield())
		{
			cBuffEffect.m_nHP = finhp * (-1);
			cAvatarPtr->changeHp(cBuffEffect.m_nHP);
		}
		
		vecBuffEffect.push_back(cBuffEffect);
	}

	return ret;
}

/*
*@校验格子的玩家是否存活
*/
bool CCombatMgr::isSurvivalGrid(std::vector<CFormatUint> &vecFormat, std::map<OBJID, CFightObjPtr> &mapBlowObj, uint8 grid)
{
	for (size_t i = 0; i < vecFormat.size(); i++)
	{
		CFormatUint &cUint = vecFormat[i];
		if (cUint.m_nGrid == grid)
		{
			OBJID idObj = cUint.m_nFightId;
			std::map<OBJID, CFightObjPtr>::iterator iter =  mapBlowObj.find(idObj);
			if (iter == mapBlowObj.end())
				return false;

			CFightObjPtr cFightObjPtr = iter->second;
			if (cFightObjPtr == NULL)
				return false;

			if(cFightObjPtr->isDie())
				return false;

			return true;
		}
	}

	return false;

}

/*
*@出战单位的技能处理
*/
bool CCombatMgr::skillEffectProc(CFightObjPtr cAvatarPtr, uint32 idSkill,  uint32 idEffect, std::vector<CFormatUint> &vecFormat, 
								 std::vector<CFormatUint> &vecRecFormat, std::map<OBJID, CFightObjPtr> &mapFightObject, 
								 uint8 skill_obj_choice, uint8 &skill_grid, CAttackWave &cWave, std::vector<uint32> &vecChaseEffectId)
{

	//根据技能效果，获取技能信息
	const Json::Value& effectConf = g_pConsummer->effectConfig(idEffect);
	if (effectConf == Json::nullValue)
		return false;
    
	//判断是否还有存活的对象
	if(! this->isHavaAlive(vecRecFormat, mapFightObject) )
		return false;

	//找到受击放的位置
	uint8 effectTarget = effectConf["effect_target"].asUInt();
	uint16 judgeType = effectConf["judge_type"].asUInt();
	uint8 rangType = effectConf["range_type"].asUInt();
    
	//使用的效果
	cWave.m_nEffectID = idEffect;

	//查找受击对象
	uint8 blowgrid = 0;
	std::map<OBJID, CFightObjPtr> mapBlowObj;
	if (effectTarget == EFFECT_TARGET_OWNER_TEAM)  //对己方
	{
		if( skill_grid != 127 )
		{
			//如果是技能选择需要同一个目标
			if( skill_obj_choice == 1 )
			{
				if( this->isSurvivalGrid(vecFormat, mapFightObject, skill_grid) )
					blowgrid = skill_grid;
				else //该目标已死亡
					return false;
			}
			else
			{
				this->getBlowGrid(vecFormat, mapFightObject, cAvatarPtr->getGrid(), judgeType, blowgrid);
			}
		}
		else
		{
			this->getBlowGrid(vecFormat, mapFightObject, cAvatarPtr->getGrid(), judgeType, blowgrid);
		}

		this->getBlowObj(vecFormat, blowgrid, rangType, mapFightObject, mapBlowObj);
	}
	else if (effectTarget == EFFECT_TARGET_ENEMY) //对敌方产生效果
	{
		if( skill_grid != 127 )
		{
			//如果是技能选择需要同一个目标
			if( skill_obj_choice == 1 )
			{
				if( this->isSurvivalGrid(vecRecFormat, mapFightObject, skill_grid) )
					blowgrid = skill_grid;
				else //该目标已死亡
					return false;
			}
			else
			{
				this->getBlowGrid(vecRecFormat, mapFightObject, cAvatarPtr->getGrid(), judgeType, blowgrid);
			}
		}
		else
		{
			this->getBlowGrid(vecRecFormat, mapFightObject, cAvatarPtr->getGrid(), judgeType, blowgrid);
		}

		this->getBlowObj(vecRecFormat, blowgrid, rangType, mapFightObject, mapBlowObj);
	}
	else  //自爆兵使用
	{
		blowgrid = 0;
		this->getAllFightObj(vecRecFormat, vecFormat, mapFightObject, mapBlowObj, blowgrid);
	}

	skill_grid = blowgrid;

	cWave.m_nTarget = effectTarget;
	cWave.m_nTargetGrid = blowgrid;
    
	//对受影响对象进行属性变化处理
	this->calBlowEffect(cAvatarPtr, blowgrid, mapBlowObj, effectConf, cWave);

	//判断是否有"追击"效果
	uint16 effectType = effectConf["affect_type"].asUInt();
	uint32 effectValues = effectConf["affect_value"].asUInt();

	if( effectType == RESULT_CHASE)
	{
		//判断敌对单位是否死亡
		if( !this->isSurvivalGrid(vecRecFormat, mapFightObject, blowgrid) )
		{
			vecChaseEffectId.push_back(effectValues);
		}
	}
	else if( effectType == RESULT_CHANCE_CHASE )
	{
		std::string strValueExt = effectConf["affect_ext"].asString();
		uint32 nRate = (uint32)atoi(strValueExt.c_str());
		size_t nRandNum = CRandom::rand(10000) + 1;
		if( nRandNum <= nRate )
			vecChaseEffectId.push_back(effectValues);
	}

	return true;
}
double CCombatMgr::calAttackTypeDemageRate(CFightObjPtr pAttacker, CFightObjPtr pDefender, uint8 nSkillHurtType)
{
	double attackDamage = 1.0;
	if (NULL == pAttacker || NULL == pDefender)
	{
		return attackDamage;
	}

	double nBaseVal = 10000.0;
	if (!nSkillHurtType)
	{
		int32 nAttackAttrRate = pAttacker->getAttr(ATTR_P_AddHurtRate);
		int32 nBlowAttrRate	= pDefender->getAttr(ATTR_P_DecHurtRate);
		attackDamage =  attackDamage + (double)(nAttackAttrRate - nBlowAttrRate) / nBaseVal;
	}
	else
	{
		int32 nAttackAttrRate = pAttacker->getAttr(ATTR_M_AddHurtRate);
		int32 nBlowAttrRate	= pDefender->getAttr(ATTR_M_DecHurtRate);
		attackDamage = attackDamage + (double)(nAttackAttrRate - nBlowAttrRate) / nBaseVal;
	}

	this->adjhurtrate(attackDamage);
	return attackDamage;
}
double CCombatMgr::calSexDemageRate(CFightObjPtr pAttacker, CFightObjPtr pDefender)
{
	double dSexRate = 0.0;
	if (NULL == pAttacker || NULL == pDefender)
		return 1.0;

	uint8 nAttackSex = pAttacker->getSex();
	uint8 nDefeneSex = pDefender->getSex();
	int32 nAttackerDat = 0, nDefenerDat = 0;
	double nBaseVal = 10000.0;
	if (IS_MALE(nAttackSex) && IS_MALE(nDefeneSex))
	{
		nAttackerDat = pAttacker->getAttr(ATTR_Male_AddHurtRate);
		nDefenerDat = pDefender->getAttr(ATTR_Male_DecHurtRate);
		dSexRate =  (double)(nAttackerDat - nDefenerDat) / nBaseVal;
	}
	else if (IS_FEMALE(nAttackSex) && IS_FEMALE(nDefeneSex))
	{
		nAttackerDat = pAttacker->getAttr(ATTR_Female_AddHurtRate);
		nDefenerDat = pDefender->getAttr(ATTR_Female_DecHurtRate);
		dSexRate = (double)(nAttackerDat - nDefenerDat) / nBaseVal;
	}
	else if (IS_MALE(nAttackSex) && IS_FEMALE(nDefeneSex))
	{
		nAttackerDat = pAttacker->getAttr(ATTR_Female_AddHurtRate);
		nDefenerDat = pDefender->getAttr(ATTR_Male_DecHurtRate);
		dSexRate =  (double)(nAttackerDat - nDefenerDat) / nBaseVal;
	}
	else if (IS_FEMALE(nAttackSex) && IS_MALE(nDefeneSex))
	{
		nAttackerDat = pAttacker->getAttr(ATTR_Male_AddHurtRate);
		nDefenerDat = pDefender->getAttr(ATTR_Female_DecHurtRate);
		dSexRate = (double)(nAttackerDat - nDefenerDat) / nBaseVal;
	}
	dSexRate += 1.0;
	this->adjhurtrate(dSexRate);
	return dSexRate;
}

//计算技能远程、近战产生的伤害加成和减免
double CCombatMgr::calDistDemageRate(CFightObjPtr pAttacker, CFightObjPtr pDefender, uint8 nAttDistType)
{
	if (NULL == pAttacker || NULL == pDefender)
		return 1.0;

	double dRate = 0.0;
	double nBaseVal = 10000.0;
	int32 nAttackDat = 0;
	int32 nDefenerDat = 0;

	if (1 == nAttDistType)
	{
		//近战
		nAttackDat = pAttacker->getAttr(ATTR_ShortDistAddHurt);
		nDefenerDat = pDefender->getAttr(ATTR_ShortDistDecHurt);
		dRate = (double)(nAttackDat - nDefenerDat) / nBaseVal;
	}
	else
	{
		//远程
		nAttackDat = pAttacker->getAttr(ATTR_LongDistAddHurt);
		nDefenerDat = pDefender->getAttr(ATTR_LongDistDecHurt);
		dRate = (double)(nAttackDat - nDefenerDat) / nBaseVal;
	}


	dRate += 1.0;
	this->adjhurtrate(dRate);
	return dRate;
}

//失血加伤、防系数
double CCombatMgr::calLoseHpAddAttrRatio(double dCurHp, double dMaxHp, uint32 nAttrValue)
{
	if( dCurHp > dMaxHp || dMaxHp == 0 )
		return 0;

	double dRatio = (double)nAttrValue * ( 1 - dCurHp/dMaxHp );
	if( dRatio > 10000 ) //系数最大值
		dRatio = 10000;

	return dRatio;
}

/**
* @功能：计算伤害
* @输入：pattackPtr 攻击方，nskillID 技能伤害，hurtratio 技能伤害系数 nSkillHurtType 伤害类型 pdefendPtr 防守方, nSkillDistType技能距离类型（1近战、2远程）
* @输出：bool目标是否在当前对象的仇恨列表第一位
* @返回：伤害值
*/
uint32 CCombatMgr::calhurt(CFightObjPtr pattackPtr, uint32 nSkillHurt, double hurtratio, uint8 nSkillHurtType, uint8 nSkillDistType,
						   uint8 attackType, CFightObjPtr pdefendPtr, uint8 &nGritFlag)
{
	/* ( (a*当前兵力/兵力上限+(1-a)) * (攻击方攻击*RAND()*10%+攻击方攻击*95%+技能附加攻击力)^2 /  
	     (攻击方攻击*RAND()*10%+攻击方攻击*95%+技能附加攻击力+受击方防御*(1-(攻击方忽视防御-受击方抗忽视))) * 
		 (1+攻击方暴击系数) * (1+攻击星级加成伤害 - 受击星级减免伤害) * (1+攻击方三围伤害加成-受击方三围伤害减免) * 
		 (1+攻击方伤害加成百分比-受击方伤害减免百分比) * (1+相克伤害加成百分比) * (1-被克伤害减免百分比)*技能伤害系数  ) -- 预留
	     + 攻击方伤害加成固定值-受击方伤害减免固定值 -- 预留
		 +RAND()*5 + 1
    */

	//衰减因子a由策划配置(浮点数，0~1)

	uint32 radomvalues = CRandom::rand(10000);
#ifdef WIN32
     radomvalues = 10000;
#endif
	 if( g_DaServer->getSettingUint32("machine settings", "combat no random", "0") != 0 )
	 {
		 log_info("CCombatMgr::calhurt combat no random !!!");
		 radomvalues = 10000;
	 }

	double skillhurt = (double)nSkillHurt;

	double decayFactor = g_pConsummer->getCambatRate(HURTCAL_DecayFactor) / 10000;  //衰减因子

	double hurtvalues = 0;

	double curHP = (double)pattackPtr->getHp();                              //当前兵力
	double maxHP = (double)pattackPtr->getAttr(ATTR_Hp);                     //兵力上限
	double attackDamage = (double)pattackPtr->getAttr(ATTR_Damage);          //攻击方攻击

	//背水效果（攻击者失血加buff）
	attackDamage += attackDamage * calLoseHpAddAttrRatio(curHP, maxHP, pattackPtr->getAttr(ATTR_LoseHpAddHurt)) / 10000.0;

	//反击
	if (attackType == 1) 
		attackDamage = attackDamage * g_pConsummer->getCambatRate(HURTCAL_StrikeBackRatio) / 10000.0;

	double beattackCurHp = (double)pdefendPtr->getHp();
	double beattackMaxHp = (double)pdefendPtr->getAttr(ATTR_Hp);
	double beattackdefend = 0; //受击方防御
	if (nSkillHurtType == 0)  //0物理伤害 1法力伤害
	{
		beattackdefend = (double)pdefendPtr->getAttr(ATTR_DefP);       //物理防御

		//背水效果（受击者失血加buff）
		beattackdefend += beattackdefend * calLoseHpAddAttrRatio(beattackCurHp, beattackMaxHp, pdefendPtr->getAttr(ATTR_LoseHpAddDefP)) / 10000.0;
	}
	else 
	{
		beattackdefend = (double)pdefendPtr->getAttr(ATTR_DefM);         //法术防御

		//背水效果（受击者失血加buff）
		beattackdefend += beattackdefend * calLoseHpAddAttrRatio(beattackCurHp, beattackMaxHp, pdefendPtr->getAttr(ATTR_LoseHpAddDefM)) / 10000.0;
	}

	double ignoreDefend = (double)pattackPtr->getAttr(ATTR_IgnoreDef);  //攻击方忽视防御
	double resistIgnoreDefend = (double)pdefendPtr->getAttr(ATTR_ResistIgnoreDef); //受击方抗忽视

	/******  判断暴击处理 begin */
	//(暴击值-抗暴值)/(10000+等级*暴击参数)
	int32 critcaldiff = (pattackPtr->getAttr(ATTR_Critical) > pdefendPtr->getAttr(ATTR_Block)) ? (pattackPtr->getAttr(ATTR_Critical) - pdefendPtr->getAttr(ATTR_Block)) : 0;
	int32 critcal_deno = (10000 + pattackPtr->getLevel() * g_pConsummer->getCambatParam(HURTCAL_PRAM_CRIT) /10000);
	int32 ncriticavalues = int32(critcaldiff * 10000 / critcal_deno );
    int32 radomcritical = CRandom::rand(10000);
	if (radomcritical < ncriticavalues)
        nGritFlag = 1;
    
	/**判断暴击处理 end **/
	double critvalus = double(pattackPtr->getAttr(ATTR_CriticalDamage) - pdefendPtr->getAttr(ATTR_CriticalReduce));
	if (critvalus < 0)
		critvalus = 0;
    
	//暴击伤害
	double criticalDamage = g_pConsummer->getCambatRate(HURTCAL_CriticalRate)/100.0 + critvalus / 10000.0;  

	//星级加成(减免)伤害 = 星级*参数*伤害加成(减免)系数/(星级+参数)
	double starthurt = 0.0;
	double attackstaradd = 0.0;
	double attackstartdel = 0.0;
	double startParam  = (double)g_pConsummer->getCambatParam(HURTCAL_PRAM_STAR); //星级参数
	if (nSkillHurtType == 0)  //0物理伤害 1法力伤害
	{
		attackstaradd = (double)(pattackPtr->getStar() * startParam * g_pConsummer->getCambatRate(HURTCAL_StartP_A) /(pattackPtr->getStar() + startParam));
		attackstartdel = (double)(pdefendPtr->getStar() * startParam * g_pConsummer->getCambatRate(HURTCAL_StartP_D) /(pdefendPtr->getStar() + startParam));
	}
	else
	{
		attackstaradd = (double)(pattackPtr->getStar() * startParam * g_pConsummer->getCambatRate(HURTCAL_StartM_A)  /(pattackPtr->getStar() + startParam));
		attackstartdel = (double)(pdefendPtr->getStar() * startParam * g_pConsummer->getCambatRate(HURTCAL_StartM_D) /(pdefendPtr->getStar() + startParam));
	}
	
	//(1+(攻击星级加成伤害 - 受击星级减免伤害)) 
	starthurt = 1.0 + (attackstaradd - attackstartdel) /10000.0;
	this->adjhurtrate(starthurt);

    //攻击方三围伤害加成
	double attack3Dumesion = 0;
	double defend3Dumesion = 0;
	if (nSkillHurtType == 0)  //0物理伤害 1法力伤害
	{
		attack3Dumesion = (double)(pattackPtr->getAttr(ATTR_Valiant) * g_pConsummer->getCambatParam(HURTCAL_PRAM_3WEI) * g_pConsummer->getCambatRate(HURTCAL_ValiantP_A)) /
			(double)(pattackPtr->getAttr(ATTR_Valiant) + g_pConsummer->getCambatParam(HURTCAL_PRAM_3WEI));   //勇武物理伤害增加 ()
		defend3Dumesion = (double)(pdefendPtr->getAttr(ATTR_Valiant) * g_pConsummer->getCambatParam(HURTCAL_PRAM_3WEI) * g_pConsummer->getCambatRate(HURTCAL_ValiantP_D)) /
			(double)(pdefendPtr->getAttr(ATTR_Valiant) + g_pConsummer->getCambatParam(HURTCAL_PRAM_3WEI)) +  //勇武物理伤害减免
			              (double)(pdefendPtr->getAttr(ATTR_Commander) * g_pConsummer->getCambatParam(HURTCAL_PRAM_3WEI) * g_pConsummer->getCambatRate(HURTCAL_CommanderP_D)) /
			(double)(pdefendPtr->getAttr(ATTR_Commander) + g_pConsummer->getCambatParam(HURTCAL_PRAM_3WEI));       //统帅物理伤害减免  
	}
	else 
	{
		attack3Dumesion = (double)(pattackPtr->getAttr(ATTR_Wisdom) * g_pConsummer->getCambatParam(HURTCAL_PRAM_3WEI) * g_pConsummer->getCambatRate(HURTCAL_WisdomM_A)) /
			(double)(pattackPtr->getAttr(ATTR_Wisdom) + g_pConsummer->getCambatParam(HURTCAL_PRAM_3WEI));   //智谋法术伤害增加 ()
		defend3Dumesion = (double)(pdefendPtr->getAttr(ATTR_Wisdom) * g_pConsummer->getCambatParam(HURTCAL_PRAM_3WEI) * g_pConsummer->getCambatRate(HURTCAL_WisdomM_D)) /
			(double)(pdefendPtr->getAttr(ATTR_Wisdom) + g_pConsummer->getCambatParam(HURTCAL_PRAM_3WEI)) +  //智谋法术伤害减免
			(double)(pdefendPtr->getAttr(ATTR_Commander) * g_pConsummer->getCambatParam(HURTCAL_PRAM_3WEI) * g_pConsummer->getCambatRate(HURTCAL_CommanderM_D)) /
			(double)(pdefendPtr->getAttr(ATTR_Commander) + g_pConsummer->getCambatParam(HURTCAL_PRAM_3WEI));       //统帅法术伤害减免  
	}

	attack3Dumesion = attack3Dumesion / 10000.0;
	defend3Dumesion = defend3Dumesion / 10000.0;

	double hphurt = (decayFactor * curHP / maxHP) + (1-decayFactor);  
	//攻击整体 (攻击方攻击*RAND()*10%+攻击方攻击*95%+技能附加攻击力)
	attackDamage = (attackDamage * radomvalues/10000.0 * 10.0 / 100.0 + attackDamage * 95.0 /100.0 + skillhurt);

	//（攻击*攻击参数）^2/(攻击*攻击参数+防御*防御参数）
	double attackhurt = attackDamage * g_pConsummer->getCambatParam(HURTCAL_PRAM_ATTACK);
	attackhurt = attackhurt * attackhurt;
    
	//整体防御
	double bothhurt =  beattackdefend * (1.0-(ignoreDefend - resistIgnoreDefend)/10000);
    bothhurt = attackDamage  * g_pConsummer->getCambatParam(HURTCAL_PRAM_ATTACK) + bothhurt * g_pConsummer->getCambatParam(HURTCAL_PRAM_DEF);
	double Dumesionhurt = 1.0 + attack3Dumesion - defend3Dumesion;
	this->adjhurtrate(Dumesionhurt);
    
	radomvalues = CRandom::rand(10000);
#ifdef WIN32
	radomvalues = 10000;
#endif
	if( g_DaServer->getSettingUint32("machine settings", "combat no random", "0") != 0 )
	{
		log_info("CCombatMgr::calhurt combat no random !!!");
		radomvalues = 10000;
	}

	double drandvalues = radomvalues * 1.0;

	//伤害加成百分比
	double demageRate = 0.0;
	//攻击方伤害加成百分比
	int32 nAttDamageP = pattackPtr->getAttr(ATTR_Damage_P);
	//防守方伤害减免百分比
	int32 nBlowDamageDecP = pdefendPtr->getAttr(ATTR_Damage_Dec_P);

	demageRate = 1.0 + (double)(nAttDamageP - nBlowDamageDecP) / 10000.0;
	this->adjhurtrate(demageRate);

	//计算兵种克制伤害加成 
	double dInitRestVal = 0.0;
	double dGrowRestVal = 0;
	double dproflvel = (pattackPtr->getProfessionLevel() - pdefendPtr->getProfessionLevel());
	this->getProfRestrain(pattackPtr->getProfession(), pdefendPtr->getProfession(), dInitRestVal, dGrowRestVal);
	double dprofRestHurt = (dInitRestVal + dproflvel * dGrowRestVal) / 10000.0;
	//增加兵种伤害增减
	dprofRestHurt = this->getProfessionHurt(pattackPtr, pdefendPtr, dprofRestHurt);
	
    //攻击方固定伤害加成
	int32 nAttFitDamage = pattackPtr->getAttr(ATTR_Damage_F);
    //防守方固定伤害减免
	int32 nBlowDamageDec = pdefendPtr->getAttr(ATTR_Damage_Dec_F);
	int32 hurtDamage = nAttFitDamage - nBlowDamageDec;
	if (nGritFlag != 1)
	    hurtvalues = hphurt * attackhurt / bothhurt * starthurt * Dumesionhurt * demageRate * dprofRestHurt * hurtratio  + hurtDamage;
	else
        hurtvalues = hphurt * attackhurt / bothhurt * (1.0 + criticalDamage) * starthurt * Dumesionhurt * demageRate * dprofRestHurt * hurtratio + hurtDamage;

	//log_debug("CCombatMgr::calhurt| hphurt[%lf], attackhurt[%lf], bothhurt[%lf], starthurt[%lf], Dumesionhurt[%lf], demageRate[%lf], dprofRestHurt[%lf], hurtratio[%lf], hurtDamage[%d]", 
	//	hphurt, attackhurt, bothhurt, starthurt, Dumesionhurt, demageRate, dprofRestHurt, hurtratio, hurtDamage);

	if (hurtvalues < 0)
		hurtvalues = 0;
	double sexDemageRate = this->calSexDemageRate(pattackPtr, pdefendPtr);
	double fixdemageRate = this->calAttackTypeDemageRate(pattackPtr, pdefendPtr, nSkillHurtType);
	double distDemageRate = this->calDistDemageRate(pattackPtr, pdefendPtr, nSkillDistType);
	hurtvalues = hurtvalues *  sexDemageRate * fixdemageRate * distDemageRate;

	hurtvalues += (drandvalues / 10000.0 * 5.0 + 1.0);
    return (uint32)hurtvalues;
}

/*
*@计算兵种伤害
*/
double CCombatMgr::getProfessionHurt(CFightObjPtr cAttObjPtr, CFightObjPtr cDefObjPtr, double dprofRestHurt)
{
	double attaddhurt = 0.0;
	double defdechurt = 0.0;
	double dprofHurt = 0.0;

	uint8 defProfession = cDefObjPtr->getProfession();
	switch (defProfession)
	{
	case PROFESSION_DAOBING:
		{
			attaddhurt = (double)cAttObjPtr->getAttr(ATTR_WeaponAddHurt);
		}
		break;
	case PROFESSION_QIANGBING:
		{
			attaddhurt = (double)cAttObjPtr->getAttr(ATTR_MarinesAddHurt);
		}
		break;
	case PROFESSION_QIBING:
		{
			attaddhurt = (double)cAttObjPtr->getAttr(ATTR_HorseAddHurt);
		}
		break;
	case PROFESSION_GONGBING:
		{
			attaddhurt = (double)cAttObjPtr->getAttr(ATTR_ArcherAddHurt);
		}
		break;
	case PROFESSION_JIXIE:
		{
			attaddhurt = (double)cAttObjPtr->getAttr(ATTR_MachineAddHurt);
		}
		break;
	case PROFESSION_WUNIANG:
		{
			attaddhurt = (double)cAttObjPtr->getAttr(ATTR_DancerAddHurt);
		}
		break;
	case PROFESSION_YISHI:
		{
			attaddhurt = (double)cAttObjPtr->getAttr(ATTR_DoctorAddHurt);
		}
		break;
	case PROFESSION_FASHI:
		{
			attaddhurt = (double)cAttObjPtr->getAttr(ATTR_MasterAddHurt);
		}
		break;
	default :
		break;
	}

	uint8 attProfession = cAttObjPtr->getProfession();
	switch (attProfession)
	{
	case PROFESSION_DAOBING:
		{
			defdechurt = (double)cDefObjPtr->getAttr(ATTR_WeaponDecHurt);
		}
		break;
	case PROFESSION_QIANGBING:
		{
			defdechurt = (double)cDefObjPtr->getAttr(ATTR_MarinesDecHurt);
		}
		break;
	case PROFESSION_QIBING:
		{
			defdechurt = (double)cDefObjPtr->getAttr(ATTR_HorseDecHurt);
		}
		break;
	case PROFESSION_GONGBING:
		{
			defdechurt = (double)cDefObjPtr->getAttr(ATTR_ArcherDecHurt);
		}
		break;
	case PROFESSION_JIXIE:
		{
			defdechurt = (double)cDefObjPtr->getAttr(ATTR_MachineDecHurt);
		}
		break;
	case PROFESSION_WUNIANG:
		{
			defdechurt = (double)cDefObjPtr->getAttr(ATTR_DancerDecHurt);
		}
		break;
	case PROFESSION_YISHI:
		{
			defdechurt = (double)cDefObjPtr->getAttr(ATTR_DoctorDecHurt);
		}
		break;
	case PROFESSION_FASHI:
		{
			defdechurt = (double)cDefObjPtr->getAttr(ATTR_MasterDecHurt);
		}
		break;
	default :
		break;
	}

	dprofHurt = dprofRestHurt + attaddhurt/10000.0 - defdechurt/10000.0;
	this->adjhurtrate(dprofHurt);

	return dprofHurt;
}

//获取单体的对象
void CCombatMgr::getOnlyBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, 
								std::map<OBJID, CFightObjPtr> &mapBlowObj)
{
	for (size_t i = 0; i < vecFormation.size(); i++)
	{
		CFormatUint &cUint = vecFormation[i];
		if (cUint.m_nGrid == 16)
			continue;
		if (cUint.m_nGrid == grid)
		{
			OBJID idBuddy = cUint.m_nFightId;
			std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(idBuddy);
			if (iter != mapFightObject.end())
				mapBlowObj.insert(make_pair(idBuddy, iter->second));
		}
	}

	return;
}

//单列 -- 以自己为目标，纵列(竖)
void CCombatMgr::getOneRankBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, 
								std::map<OBJID, CFightObjPtr> &mapBlowObj)
{
	uint8 ngrid = grid;
	for (size_t i = 0; i < vecFormation.size(); i++)
	{
		CFormatUint &cUint = vecFormation[i];
		if (cUint.m_nGrid == 16)
			continue;
		if ((cUint.m_nGrid)%4 == ngrid%4)
		{
			OBJID fightid = cUint.m_nFightId;
			std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
			if (iter != mapFightObject.end())
			{
				CFightObjPtr cfightObjPtr = iter->second;
				if (!cfightObjPtr->isDie())
					mapBlowObj.insert(make_pair(fightid, cfightObjPtr));
			}
		}
	}

	return;
}

//单行 -- 以自己为目标，横排
void CCombatMgr::getOneLineBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, 
								   std::map<OBJID, CFightObjPtr> &mapBlowObj)
{
	uint8 ngrid = grid;
	for (size_t i = 0; i < vecFormation.size(); i++)
	{
		CFormatUint &cInf = vecFormation[i];
		if (cInf.m_nGrid == 16)
			continue;
		if ((cInf.m_nGrid)/4 == ngrid/4)
		{
			OBJID fightid = cInf.m_nFightId;
			std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
			if (iter != mapFightObject.end())
			{
				CFightObjPtr cfightObjPtr = iter->second;
				if (!cfightObjPtr->isDie())
					mapBlowObj.insert(make_pair(fightid, cfightObjPtr));
			}
		}
	}

	return;
}

//十字 -- 以自己为目标，十字
void CCombatMgr::getCrossBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, 
								   std::map<OBJID, CFightObjPtr> &mapBlowObj)
{
	int8 ngrid = grid;
	for (size_t i = 0; i < vecFormation.size(); i++)
	{
		CFormatUint &cUint = vecFormation[i];
		if (cUint.m_nGrid == 16)
			continue;
		int8 cgrid = (int8)cUint.m_nGrid;
		if ( ((cgrid+4) == ngrid)|| ((cgrid-4) == ngrid)||
			((cgrid+1) == ngrid) || ((cgrid-1) == ngrid) ||
			(cgrid == ngrid) ) 
		{
			if ( (ngrid%4 == 0) && ((cgrid+1) == ngrid) )
				continue;
			if ( (ngrid%4 == 3) && ((cgrid-1) == ngrid))
				continue;

			OBJID fightid = cUint.m_nFightId;
			std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
			if (iter != mapFightObject.end())
			{
				CFightObjPtr cfightObjPtr = iter->second;
				if (!cfightObjPtr->isDie())
					mapBlowObj.insert(make_pair(fightid, cfightObjPtr));
			}
		}
	}

	return;
}

//两翼
void CCombatMgr::getCountBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, 
								 std::map<OBJID, CFightObjPtr> &mapBlowObj)
{
	int8 ngrid = grid;
	for (size_t i = 0; i < vecFormation.size(); i++)
	{
		CFormatUint &cUint = vecFormation[i];
		if (cUint.m_nGrid == 16)
			continue;
		int8 cgrid = (int8)cUint.m_nGrid;
		if ( ((cgrid-5) == ngrid) || ((cgrid-3) == ngrid)|| (cgrid == ngrid)) 
		{
			if ( (grid % 4 == 0) && ((cgrid-3) == ngrid) )
				continue;
			if ( (grid % 4 == 3) && ((cgrid-5) == ngrid) )
				continue;

			OBJID fightid = cUint.m_nFightId;
			std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
			if (iter != mapFightObject.end())
			{
				CFightObjPtr cfightObjPtr = iter->second;
				if (!cfightObjPtr->isDie())
					mapBlowObj.insert(make_pair(fightid, cfightObjPtr));
			}
		}
	}

	return;
}

//X型
void CCombatMgr::getXBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, 
								 std::map<OBJID, CFightObjPtr> &mapBlowObj)
{
	int8 ngrid = grid;
	for (size_t i = 0; i < vecFormation.size(); i++)
	{
		CFormatUint &cUint = vecFormation[i];
		if (cUint.m_nGrid == 16)
			continue;
		int8 cgrid = (int8)cUint.m_nGrid;
		if ( ((cgrid+5) == ngrid)|| ((cgrid-5) == ngrid)||
			((cgrid+3) == ngrid)|| ((cgrid-3) == ngrid) ||
			(cgrid == ngrid)) 
		{
			if ( (grid % 4 == 0) &&  ( (cgrid-3) == ngrid || (cgrid+5) == ngrid ) )
				continue;
			if ( (grid % 4 == 3) &&  ( (cgrid-5) == ngrid || (cgrid+3) == ngrid ) )
				continue;

			OBJID fightid = cUint.m_nFightId;
			std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
			if (iter != mapFightObject.end())
			{
				CFightObjPtr cfightObjPtr = iter->second;
				if (!cfightObjPtr->isDie())
					mapBlowObj.insert(make_pair(fightid, cfightObjPtr));
			}
		}
	}

	return;
}

//群体
void CCombatMgr::getAllBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, 
								 std::map<OBJID, CFightObjPtr> &mapBlowObj)
{
	for (size_t i = 0; i < vecFormation.size(); i++)
	{
		CFormatUint &cUint = vecFormation[i];
		if (cUint.m_nGrid == 16)
			continue;
		OBJID fightid = cUint.m_nFightId;
		std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
		if (iter != mapFightObject.end())
		{
			CFightObjPtr cfightObjPtr = iter->second;
			if (!cfightObjPtr->isDie())
				mapBlowObj.insert(make_pair(fightid, cfightObjPtr));
		}
	}

	return;
}

//3*3 -- 以自己为目标，3*3
void CCombatMgr::getAroundBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, 
							   std::map<OBJID, CFightObjPtr> &mapBlowObj)
{
	int8 ngrid = grid;
	int8 nmod = (grid+1) % 4;
	for (size_t i = 0; i < vecFormation.size(); i++)
	{
		CFormatUint &cUint = vecFormation[i];
		if (cUint.m_nGrid == 16)
			continue;
		int8 cgrid = (int8)cUint.m_nGrid;
		OBJID fightid = 0;
		switch(nmod)
		{
		case 0:
			{
				if (((cgrid+4) == ngrid)|| ((cgrid-4) == ngrid) ||
					( (cgrid-3) == ngrid || (cgrid+5) == ngrid ) ||
					( (cgrid+1) == ngrid ) ||
					(cgrid == grid)
					)
					fightid = cUint.m_nFightId;
			}
			break;
		case 1:
			{
				if (  ((cgrid+4) == ngrid || (cgrid-4) == ngrid ) ||
					( (cgrid-5) == ngrid || (cgrid+3) == ngrid ) ||
					( (cgrid-1) == ngrid ) ||
					(cgrid == grid)
					)
					fightid = cUint.m_nFightId;
			}
			break;
		default:
			{
				if ( ((cgrid+3) == ngrid)|| ((cgrid-3) == ngrid)||
					( (cgrid+5) == ngrid || (cgrid-5) == ngrid ) ||
					( (cgrid+4) == ngrid || (cgrid-4) == ngrid ) ||
					( (cgrid+1) == ngrid || (cgrid-1) == ngrid ) ||
					(cgrid == grid)
					)
					fightid = cUint.m_nFightId;
			}
			break;
		}
		if (fightid > 0)
		{
			std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
			if (iter != mapFightObject.end())
			{
				CFightObjPtr cfightObjPtr = iter->second;
				if (!cfightObjPtr->isDie())
					mapBlowObj.insert(make_pair(fightid, cfightObjPtr));
			}
		}
	}

	return;
}

//随机两个目标
void CCombatMgr::getRand2BlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, 
								  std::map<OBJID, CFightObjPtr> &mapBlowObj)
{
	int8 ngrid = grid;
	//找出存活的所有目标
	std::vector<OBJID> vecPosLive;
	for (size_t i = 0; i < vecFormation.size(); i++)
	{
		CFormatUint &cUint = vecFormation[i];
		if (cUint.m_nGrid == 16)
			continue;
		int8 cgrid = (int8)cUint.m_nGrid;
		if (cgrid == ngrid) //指定目标
		{
			OBJID fightid = cUint.m_nFightId;
			std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
			if (iter != mapFightObject.end())
			{
				CFightObjPtr cfightObjPtr = iter->second;
				if (!cfightObjPtr->isDie())
					mapBlowObj.insert(make_pair(fightid, iter->second));
			}
		}
		else
		{
			OBJID fightid = cUint.m_nFightId;
			std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
			if (iter != mapFightObject.end())
			{
				CFightObjPtr cfightObjPtr = iter->second;
				if (!cfightObjPtr->isDie())
					vecPosLive.push_back(fightid);
			}
		}
	}

	//如果存活只有小于两个，则直接获取结果
	if (vecPosLive.size() <= 2)
	{
		for (size_t i = 0; i< vecPosLive.size(); i++)
		{
			OBJID fightid = vecPosLive[i];
			std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
			if (iter != mapFightObject.end())
				mapBlowObj.insert(make_pair(fightid, iter->second));
		}
	}
	else
	{
		this->getrandObj(vecPosLive, mapFightObject, mapBlowObj);
		this->getrandObj(vecPosLive, mapFightObject, mapBlowObj);
	}

	return;
}

/*
*@随机获取数据
*/
void CCombatMgr::getrandObj(std::vector<OBJID> &vecObj, std::map<OBJID, CFightObjPtr> &mapFightObject, std::map<OBJID, CFightObjPtr> &mapBlowObj)
{
	if (vecObj.size() == 0)
		return;

	OBJID fightid = 0;

	int alivenum = vecObj.size();
	int randnum = CRandom::rand(alivenum);
	if (randnum == alivenum)
		randnum = 0;

	fightid = vecObj[randnum];

	std::vector<OBJID>::iterator iter_obj = vecObj.begin();
	for(; iter_obj != vecObj.end(); ++iter_obj)
	{
		if (fightid == *iter_obj)
		{
			vecObj.erase(iter_obj);
			break;
		}
	}

	std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
	if (iter != mapFightObject.end())
		mapBlowObj.insert(make_pair(fightid, iter->second));
}

//穿刺
void CCombatMgr::getTapsBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, 
								 std::map<OBJID, CFightObjPtr> &mapBlowObj)
{
	int8 ngrid = grid;
	for (size_t i = 0; i < vecFormation.size(); i++)
	{
		CFormatUint &cUint = vecFormation[i];
		if (cUint.m_nGrid == 16)
			continue;

		int8 cgrid = (int8)cUint.m_nGrid;
		if ( ((cgrid-4) == ngrid) ||
			(cgrid == ngrid)) 
		{
			OBJID fightid = cUint.m_nFightId;
			std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
			if (iter != mapFightObject.end())
			{
				CFightObjPtr cfightObjPtr = iter->second;
				if (!cfightObjPtr->isDie())
					mapBlowObj.insert(make_pair(fightid, cfightObjPtr));
			}
		}
	}

	return;
}


//波形单位
void CCombatMgr::getWaveBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, 
									 std::map<OBJID, CFightObjPtr> &mapBlowObj)
{
	std::vector<uint8> vecGrid;
	for (size_t i = 0; i < vecFormation.size(); i++)
	{
		CFormatUint &cUint = vecFormation[i];

		//灵龙不处理
		if (cUint.m_nGrid == 16)
			continue;

		OBJID fightid = cUint.m_nFightId;

		std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
		if (iter != mapFightObject.end())
		{
			CFightObjPtr cfightObjPtr = iter->second;
			if (cfightObjPtr->isDie())
				continue;

			uint8 grid = cUint.m_nGrid;
			uint8 flag = 0;
			for(size_t j = 0; j < vecGrid.size(); j++)
			{
				uint8 wavegrid = vecGrid[j];
				//判断同意行是否已经有排前的对象
				if (wavegrid%4 == grid %4)
				{
					flag = 1;
					break;
				}
			}

			if (flag == 0)
			{
				mapBlowObj.insert(make_pair(fightid, cfightObjPtr));
				vecGrid.push_back(grid);
			}
		}
	}
	return;
}

void CCombatMgr::getAllFightObj(std::vector<CFormatUint>& vecBlowFormation, 
				   std::vector<CFormatUint>& vecAttFormation,
				   std::map<OBJID, CFightObjPtr> &mapFightObject, 
				   std::map<OBJID, CFightObjPtr> &mapBlowObj,
				   uint8 &blowgrid)
{
	for (size_t i = 0; i < vecBlowFormation.size(); i++)
	{
		CFormatUint &cUint = vecBlowFormation[i];
		if (cUint.m_nGrid == 16)
			continue;
		OBJID idBuddy = cUint.m_nFightId;
		std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(idBuddy);
		if (iter != mapFightObject.end())
		{
			blowgrid = cUint.m_nGrid; 
			mapBlowObj.insert(make_pair(idBuddy, iter->second));
		}
	}

	for (size_t i = 0; i < vecAttFormation.size(); i++)
	{
		CFormatUint &cUint = vecAttFormation[i];
		if (cUint.m_nGrid == 16)
			continue;
		OBJID idBuddy = cUint.m_nFightId;
		std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(idBuddy);
		if (iter != mapFightObject.end())
			mapBlowObj.insert(make_pair(idBuddy, iter->second));
	}
    
	return;
}

/*
*@获取承受伤害的对象
*vecFormation 受击方阵型 grid --受击对象 rang_type 受击范围 mapFightObject-战斗人员信息
*mapBlowObj 受影响人员
*/
void CCombatMgr::getBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid, uint16 rang_type,
							std::map<OBJID, CFightObjPtr> &mapFightObject, 
							std::map<OBJID, CFightObjPtr> &mapBlowObj)
{
	switch(rang_type)
	{
	case EFFECT_RANG_TYPE_ONLY: //单体
		{
			this->getOnlyBlowObj(vecFormation, grid, mapFightObject, mapBlowObj);
		}
		break;
	case EFFECT_RANG_TYPE_ONE_RANK: //单列 -- 以自己为目标，纵列(竖)
		{
			this->getOneRankBlowObj(vecFormation, grid, mapFightObject, mapBlowObj);
		}
		break;
	case EFFECT_RANG_TYPE_ONE_LINE: //单行 -- 以自己为目标，横排
		{
			this->getOneLineBlowObj(vecFormation, grid, mapFightObject, mapBlowObj);
		}
		break;
	case EFFECT_RANG_TYPE_CROSS: //十字 -- 以自己为目标，十字
		{
			this->getCrossBlowObj(vecFormation, grid, mapFightObject, mapBlowObj);
		}
		break;
	case EFFECT_RANG_TYPE_COUNT: //两翼
		{
			this->getCountBlowObj(vecFormation, grid, mapFightObject, mapBlowObj);
		}
		break;
	case EFFECT_RANG_TYPE_ALL: //群体
		{
			this->getAllBlowObj(vecFormation, grid, mapFightObject, mapBlowObj);
		}
		break;
	case EFFECT_RANG_TYPE_AROUND://3*3 -- 以自己为目标，3*3
		{
			this->getAroundBlowObj(vecFormation, grid, mapFightObject, mapBlowObj);
		}
		break;
	case EFFECT_RANG_TYPE_RAND_2:  //随机两目标
		{
			this->getRand2BlowObj(vecFormation, grid, mapFightObject, mapBlowObj);
		}
		break;
	case EFFECT_RANG_TYPE_TAPS: //穿刺
		{
			this->getTapsBlowObj(vecFormation, grid, mapFightObject, mapBlowObj);
		}
		break;
	case EFFECT_RANG_TYPE_WAVE: //波形单位
		{
            this->getWaveBlowObj(vecFormation, grid, mapFightObject, mapBlowObj);
		}
		break;
	case EFFECT_RANG_TYPE_X: //X型 
		{
            this->getXBlowObj(vecFormation, grid, mapFightObject, mapBlowObj);
		}
		break;
	default:
		break;
	}

	return;
}

//获取最近的对象
void CCombatMgr::getNearBlowGrid( uint8 attackgrid, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid)
{
	int8 srcgrid = (int8)(attackgrid%4);
	uint8 minline = 6;  //最小的行
	uint8 mindis = 20; //最小距离
	for (size_t i = 0; i < vecAliveObj.size(); i++)
	{
		CFormatUint &cGrid = vecAliveObj[i];
		int8 dstgrid = (int8)cGrid.m_nGrid;
		uint8 dstline = dstgrid/4;  //行
		if (dstline <= minline)
		{
			minline = dstline;
			dstgrid =  dstgrid % 4;
			uint8 dis = abs(dstgrid-srcgrid);
			if (dis < mindis)
			{
				blowgrid = cGrid.m_nGrid;
				mindis = dis;
			}
		}
	}
	return;
}


//远程攻击目标选取
void  CCombatMgr::getFarBlowGrid( uint8 attackgrid, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid)
{
	int8 attackLine = (int8)(attackgrid % 4);
	
	int8 minLineDis = 4;

	for( int i = vecAliveObj.size() - 1; i >= 0; i -- )
	{
		CFormatUint &cGrid = vecAliveObj[i];
		uint8 dstgrid = cGrid.m_nGrid;

		int8 dstLine = (int8)(dstgrid % 4);

		if( abs(dstLine - attackLine) < abs(minLineDis) )
		{
			blowgrid = dstgrid;
			minLineDis = dstLine - attackLine;
		}
		else if( abs(dstLine - attackLine) == abs(minLineDis) )
		{
			if( (dstLine - attackLine) <= minLineDis )
			{
				blowgrid = dstgrid;
				minLineDis = dstLine - attackLine;
			}
		}
	}

	return;
}

/*
//获取最远的对象
void  CCombatMgr::getFarBlowGrid( uint8 attackgrid, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid)
{
	int8 srcgrid = (int8)(attackgrid%4);
	uint8 maxline = 0; //最大的行
	uint8 mindis = 20;  //最小距离
	for (int i = (vecAliveObj.size() -1); i >= 0; i--)
	{
		CFormatUint &cGrid = vecAliveObj[i];
		int8 dstgrid = (int8)cGrid.m_nGrid;
		dstgrid =  dstgrid % 4;
		uint8 dstline = dstgrid/4;  //行
		if (dstline >= maxline)
		{
			maxline = dstline;
			uint8 dis = abs(dstgrid-srcgrid);
			if (dis <= mindis)
			{
				blowgrid = cGrid.m_nGrid;
				mindis = dis;
			}
		}
	}
	return;
}*/

//获取血量最低的对象
void CCombatMgr::getMinHpBlowGrid(std::map<OBJID, CFightObjPtr> &mapFightObject, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid)
{
	uint32 minHp = 99999999;
	for (size_t i = 0; i < vecAliveObj.size(); i++)
	{
		CFormatUint &cGrid = vecAliveObj[i];
		OBJID fightid = cGrid.m_nFightId;
		std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
		if (iter != mapFightObject.end())
		{
			CFightObjPtr cFightPtr = iter->second;
			if (cFightPtr->getHp() < minHp)
			{
				minHp = cFightPtr->getHp();
				blowgrid = cGrid.m_nGrid;
			}	
		}
	}

	return;
}

//获取血量最高的对象
void CCombatMgr::getMaxHpBlowGrid(std::map<OBJID, CFightObjPtr> &mapFightObject, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid)
{
	uint32 maxHp = 0;
	for (size_t i = 0; i < vecAliveObj.size(); i++)
	{
		CFormatUint &cGrid = vecAliveObj[i];
		OBJID fightid = cGrid.m_nFightId;
		std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
		if (iter != mapFightObject.end())
		{
			CFightObjPtr cFightPtr = iter->second;
			if (cFightPtr->getHp() > maxHp)
			{
				maxHp = cFightPtr->getHp();
				blowgrid = cGrid.m_nGrid;
			}	
		}
	}

	return;
}

//获取怒气最低的对象
void CCombatMgr::getMinEnBlowGrid(std::map<OBJID, CFightObjPtr> &mapFightObject,std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid)
{
	uint32 minen = 9999999;
	std::vector<uint8> vecgrid;
	for (size_t i = 0; i < vecAliveObj.size(); i++)
	{
		CFormatUint &cGrid = vecAliveObj[i];
		OBJID fightid = cGrid.m_nFightId;
		std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
		if (iter != mapFightObject.end())
		{
			CFightObjPtr cFightPtr = iter->second;
			if (cFightPtr->getEn() < minen)
				minen = cFightPtr->getEn();
		}
	}

	//找出所有怒气最低的对象
	for (size_t i = 0; i < vecAliveObj.size(); i++)
	{
		CFormatUint &cGrid = vecAliveObj[i];
		OBJID fightid = cGrid.m_nFightId;
		std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
		if (iter != mapFightObject.end())
		{
			CFightObjPtr cFightPtr = iter->second;
			if (cFightPtr->getEn() == minen)
				vecgrid.push_back(cGrid.m_nGrid);
		}
	}

	int alivenum = vecgrid.size();
	if (alivenum == 0)
	{
		blowgrid = 0;
		return;
	}

	int randnum = CRandom::rand(alivenum);
	if (randnum == alivenum)
		randnum = 0;

	blowgrid = vecgrid[randnum];
	return;
}

//获取怒气最高的对象
void CCombatMgr::getMaxEnBlowGrid(std::map<OBJID, CFightObjPtr> &mapFightObject, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid)
{
	int32 maxen = -1;
	std::vector<uint8> vecgrid;
	for (size_t i = 0; i < vecAliveObj.size(); i++)
	{
		CFormatUint &cGrid = vecAliveObj[i];
		OBJID fightid = cGrid.m_nFightId;
		std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
		if (iter != mapFightObject.end())
		{
			CFightObjPtr cFightPtr = iter->second;
			int32 en = (int32)cFightPtr->getEn();
			if (en >= maxen)
			{
				blowgrid = cGrid.m_nGrid;
				maxen = en;
			}	
		}
	}

	return;
}

//选择兵力百分比最低的单位, 百分比相同选择在阵id最小的单位
void CCombatMgr::getMinHpRateObjGrid(std::map<OBJID, CFightObjPtr> &mapFightObject, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid)
{
	double nMinHpRate = 1.0;
	double nEsp = 10e-3;

	blowgrid = 0;
	for (size_t i = 0; i < vecAliveObj.size(); ++ i)
	{
		CFormatUint &cGrid = vecAliveObj[i];
		OBJID fightid = cGrid.m_nFightId;
		std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
		if ((iter == mapFightObject.end()) || (NULL == iter->second))
		{
			continue;
		}
		CFightObjPtr cFightPtr = iter->second;
		double nCurHpRate = (double)(cFightPtr->getHp())/cFightPtr->getMaxHP();
		if (nMinHpRate > nCurHpRate)
		{
			blowgrid = cGrid.m_nGrid;
			nMinHpRate = nCurHpRate;
		}
		else if ((fabs(nMinHpRate - nCurHpRate) <= nEsp) && (!blowgrid))
		{
			blowgrid = cGrid.m_nGrid;
		}
	}
}

//获取随机的对象
void CCombatMgr::getRandBlowGrid(std::map<OBJID, CFightObjPtr> &mapFightObject, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid)
{
	int alivenum = vecAliveObj.size();
	int randnum = CRandom::rand(alivenum);
	if (randnum == alivenum)
		randnum = 0;
	if (randnum < 0)
		randnum = 0;

	for (size_t i = 0; i < vecAliveObj.size(); i++)
	{
		if (i == randnum)
		{
			CFormatUint &cGrid = vecAliveObj[i];
			OBJID fightid = cGrid.m_nFightId;
			std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
			if (iter != mapFightObject.end())
			{
				blowgrid = cGrid.m_nGrid;
				return;
			}
		}
	}

	return;
}

//获取后方的对象 (后方列距离最小，相同取id小(优先同一行))
void CCombatMgr::getBackBlowGrid(uint8 attackgrid, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid)
{
	int8 srcgrid = (int8)(attackgrid%4);
	uint8 minline = 0; //最小的行
	uint8 maxdis = 20;  //最远距离
	for (int i = (vecAliveObj.size() -1); i >= 0; i--)
	{
		CFormatUint &cGrid = vecAliveObj[i];
		int8 dstgrid = (int8)cGrid.m_nGrid;
		uint8 dstline = dstgrid/4;  //行
		if (dstline >= minline)
		{
			minline = dstline;
			dstgrid =  dstgrid % 4;
			uint8 dis = abs(dstgrid-srcgrid);
			if (dis <= maxdis)
			{
				blowgrid = cGrid.m_nGrid;
				maxdis = dis;
			}
		}
	}

	return;
}


 //优先选择不满怒的随机单位
void CCombatMgr::getNoMaxENGrid(std::map<OBJID, CFightObjPtr> &mapFightObject, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid)
{
	std::vector<CFormatUint> vecLowEn;
	for (size_t i = 0; i < vecAliveObj.size(); i++)
	{
		CFormatUint &cInf = vecAliveObj[i];
		if (cInf.m_nGrid == 16)
			continue;

		OBJID fightid = cInf.m_nFightId;
		std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
		if (iter != mapFightObject.end())
		{
			CFightObjPtr cfightObjPtr = iter->second;

			//找出未满怒的对象
			if (cfightObjPtr->getEn() < cfightObjPtr->getMaxEn())
				vecLowEn.push_back(cInf);	
		}
	}
	uint32 irand = 0;
	uint32 randvalus  = 0;

	//如果都是满怒的
	if (vecLowEn.size() == 0)
	{
		irand = vecAliveObj.size();
		randvalus = CRandom::rand(irand);
		if (randvalus == irand)
			randvalus = 0;
		blowgrid = vecAliveObj[randvalus].m_nGrid;
		return;
	}
	else
	{
		irand = vecLowEn.size();
		randvalus = CRandom::rand(irand);
		if (randvalus == irand)
			randvalus = 0;

		blowgrid = vecLowEn[randvalus].m_nGrid;
	}

	return;
}


/*
*@获取承受伤害的位置
*vecFormation 受寄方阵型 mapFightObject 战斗对象 attackgrid --攻击对象位置 attack_type 攻击类型
*blowgrid 受击对象位置
*/
void CCombatMgr::getBlowGrid(std::vector<CFormatUint> &vecFormation, std::map<OBJID, CFightObjPtr> &mapFightObject,
				              uint8 attackgrid, uint16 attack_type, uint8 &blowgrid)
{
	blowgrid = 0;
	std::vector<CFormatUint> vecAliveObj; //先找出存活的对象
	for (size_t i = 0; i < vecFormation.size(); i++)
	{
		CFormatUint &cGrid = vecFormation[i];
		if (cGrid.m_nGrid == 16) //不处理灵龙
			continue;

		OBJID fightid = cGrid.m_nFightId;
		std::map<OBJID, CFightObjPtr>::iterator iter = mapFightObject.find(fightid);
		if (iter != mapFightObject.end())
		{
			CFightObjPtr cFightPtr = iter->second;
			if (cFightPtr->getHp() > 0)
				vecAliveObj.push_back(vecFormation[i]);
		}
	}

	switch(attack_type)
	{
	case EFFECT_JUDGE_TYPE_SELF: //自己
		{
			blowgrid = attackgrid;
		}
		break;
	case EFFECT_JUDGE_TYPE_NEAR://近战
		{
			this->getNearBlowGrid(attackgrid, vecAliveObj, blowgrid);
		}
		break;
	case EFFECT_JUDGE_TYPE_FAR: //远战
		{
			this->getFarBlowGrid(attackgrid, vecAliveObj, blowgrid);
		}
		break;
	case EFFECT_JUDGE_TYPE_HP_MIN: //血量最少
		{
			this->getMinHpBlowGrid(mapFightObject, vecAliveObj, blowgrid);
		}
		break;
	case EFFECT_JUDGE_TYPE_HP_MAX: //血量最高
		{
			this->getMaxHpBlowGrid(mapFightObject, vecAliveObj, blowgrid);
		}
		break;
	case EFFECT_JUDGE_TYPE_EN_MIN: //怒气最低
		{
			this->getMinEnBlowGrid(mapFightObject, vecAliveObj, blowgrid);
		}
		break;
	case EFFECT_JUDGE_TYPE_EN_MAX: //怒气最高
		{
			this->getMaxEnBlowGrid(mapFightObject, vecAliveObj, blowgrid);
		}
		break;
	case EFFECT_JUDGE_TYPE_RAND: //随机
		{
			this->getRandBlowGrid(mapFightObject, vecAliveObj, blowgrid);
		}
		break;
	case EFFECT_JUDGE_TYPE_BACK: //后方列距离最小，相同取id小(优先同一行)
		{
			this->getBackBlowGrid(attackgrid, vecAliveObj, blowgrid);
		}
		break;
	case EFFECT_JUDGE_TYPE_FIRST: //第一个出手
		{
			if (vecAliveObj.size() > 0)
			    blowgrid = vecAliveObj[0].m_nGrid;
		}
		break;
	case EFFECT_JUDGE_TYPE_RAND_EN_LOW: //优先选择不满怒的随机单位
		{
			this->getNoMaxENGrid(mapFightObject, vecAliveObj, blowgrid);
		}
		break;
	case EFFECT_JUDGE_TYPE_MIN_HP_RATE: //当前血量百分比最低的单位
		{
			this->getMinHpRateObjGrid(mapFightObject, vecAliveObj, blowgrid);
		}
		break;
	case EFFECT_JUDGE_TYPE_LEFT_MOST: //阵型中最左边，若存在两个或以上，优先判断前排
		{
			this->getLeftMostObjGrid(mapFightObject, vecAliveObj, blowgrid);
		}
		break;
	case EFFECT_JUDGE_TYPE_RIGHT_MOST: //阵型中最右边，若存在两个或以上，优先判断前排
		{
			this->getRightMostObjGrid(mapFightObject, vecAliveObj, blowgrid);
		}
		break;
	default:
		break;
	}

	return;
}

//阵型中最左边，若存在两个或以上，优先判断前排
void CCombatMgr::getLeftMostObjGrid(std::map<OBJID, CFightObjPtr> &mapFightObject, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid)
{
	//12, 8, 4, 0 -> 4n + 0
	//13, 9, 5, 1 -> 4n + 1
	//14,10, 6, 2 -> 4n + 2
	//15,11, 7, 3 -> 4n + 3
	//vecAliveObj.m_nGrid 升序
#define ROW_SIZE 4
	int8 vecExistFlag[ROW_SIZE];
	for (int i = 0; i < ROW_SIZE; ++ i) {
		vecExistFlag[i] = -1;
	}

	for (size_t i = 0; i < vecAliveObj.size(); ++ i) {
		uint8 nGrid = vecAliveObj[i].m_nGrid;
		uint8 nBarrel = nGrid%ROW_SIZE;
		if (-1 == vecExistFlag[nBarrel]) {
			vecExistFlag[nBarrel] = nGrid;
		}
	}

	for (int i = 0; i < ROW_SIZE; ++ i) {
		if (-1 != vecExistFlag[i]) {
			blowgrid = vecExistFlag[i];
			return;
		}
	}

	if (vecAliveObj.size() > 0) {
		blowgrid = vecAliveObj[0].m_nGrid;
	}

	blowgrid = 0;
}

//阵型中最右边，若存在两个或以上，优先判断前排
void CCombatMgr::getRightMostObjGrid(std::map<OBJID, CFightObjPtr> &mapFightObject, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid)
{
	//12, 8, 4, 0 -> 4n + 0
	//13, 9, 5, 1 -> 4n + 1
	//14,10, 6, 2 -> 4n + 2
	//15,11, 7, 3 -> 4n + 3
	//vecAliveObj.m_nGrid 升序
#define ROW_SIZE 4
	int8 vecExistFlag[ROW_SIZE];
	for (int i = 0; i < ROW_SIZE; ++ i) {
		vecExistFlag[i] = -1;
	}

	for (size_t i = 0; i < vecAliveObj.size(); ++ i) {
		uint8 nGrid = vecAliveObj[i].m_nGrid;
		uint8 nBarrel = nGrid%ROW_SIZE;
		if (-1 == vecExistFlag[nBarrel]) {
			vecExistFlag[nBarrel] = nGrid;
		}
	}

	for (int i = ROW_SIZE-1; i >= 0; -- i) {
		if (-1 != vecExistFlag[i]) {
			blowgrid = vecExistFlag[i];
			return;
		}
	}

	if (vecAliveObj.size() > 0) {
		blowgrid = vecAliveObj[0].m_nGrid;
	}

	blowgrid = 0;
}

/*
*@计算受击方影响
*mapBlowObject 受击方对象 detype --效果类型 affect_type 属性增加类型 affect_value 影响值
*vecBlower 受击方结果
*/
void CCombatMgr::calBlowEffect(CFightObjPtr cAttackObj, uint8 attack_grid, std::map<OBJID, CFightObjPtr> &mapBlowObj, 
							   const Json::Value& effectConf, CAttackWave &cWave)
{
	uint32 idEffect =  effectConf["id"].asUInt();
	uint8 damageType = (uint8)effectConf["damage_type"].asUInt();
	std::string strRandBuff = effectConf["rand_buff"].asString();
	std::string strEffectPower = effectConf["effect_power"].asString();
	uint16 effectType = effectConf["affect_type"].asUInt();
	uint32 effectValues = effectConf["affect_value"].asUInt();
	int32 addEn = effectConf["add_en"].asInt();
	uint8 effectTarget = effectConf["effect_target"].asUInt();
	uint8 affectdistance =  effectConf["affect_distance"].asUInt(); //1.近战2.远程

	//判断是否有BUFF产生 
	std::vector<CRandBuff> vecaddbuff;
	if (strRandBuff.size() > 0)
	{
		std::string sSplitStr = ";";
		std::vector<std::string> vecbuff;
		vecbuff.clear();
		splitString(strRandBuff, sSplitStr, vecbuff, true);
		for (size_t i = 0; i < vecbuff.size(); i++)
		{
			 uint32 buffid = 0;
			 uint32 rate = 0;
			 if (sscanf(vecbuff[i].c_str(), "%d:%d", &rate, &buffid) != 2)
				 continue;

			 CRandBuff cInf;
			 cInf.m_nBuffID = buffid;
			 cInf.m_nRate = (uint16)rate;
			 vecaddbuff.push_back(cInf);
		}
	}

	int nblockhp = 0;
	int32 totblowhurt = 0;

	//循环处理伤害
	std::map<OBJID, CFightObjPtr>::iterator iter = mapBlowObj.begin();
	for (; iter != mapBlowObj.end(); ++iter)
	{
		CBlowUint cUint;
		CBlowUint cAttckUint;
		bool battackflag = false;
		CFightObjPtr cBlowPtr = iter->second;
		cUint.m_nFightID = cBlowPtr->getAvatarID();
		bool baddbuff = false;
		//增加BUFF
		for  (size_t i = 0; i < vecaddbuff.size(); i++)
		{
			//判断是否产生BUFF
			CRandBuff &cRandBuff = vecaddbuff[i];
			if (CRandom::rand()%(10000) <= cRandBuff.m_nRate)
			{
				uint32 idBuff = cRandBuff.m_nBuffID;
				const Json::Value& buffConf = g_pConsummer->buffConfig(idBuff);
				if (buffConf == Json::nullValue)
					continue;

				uint32 buffeff = buffConf["buff_effect"].asUInt();
				if (cBlowPtr->addBuff(idBuff, buffConf, cAttackObj->getAttr(ATTR_Damage), cUint.m_vecDelBuff))
				{
					cUint.m_vecAddBuff.push_back(idBuff);
					if (IS_ATTR_BUFF_EFFECT(buffeff))
						baddbuff =  true;
				}
			}
		}
        
		//如果有增加BUFF(属性类)，需要进行BUFF的处理
		if (baddbuff)
			cBlowPtr->updateAttr(ATTRTYPE_BUFF, true);
 
		//判断是否有圣盾，且攻击方不是世界boss 并且不是必杀的技能
		if (effectTarget == EFFECT_TARGET_ENEMY && 
			cBlowPtr->isHaveDevineShield() && 
			cAttackObj->getMonsterType() != MONSTER_TYPE_WORLD_BOSS &&
			cAttackObj->getMonsterType() != MONSTER_TYPE_MONTH_INS_BOSS &&
			damageType != EFFECT_HURT_TYPE_MUST_KILL)
		{
			CHurtValues cInf;
			cInf.m_nType = HURT_TYPE_HP;
			cInf.m_nValues = 0;
			cUint.m_vecAttr.push_back(cInf);
			cUint.m_nFlag = 0;
			cWave.m_vecBlower.push_back(cUint);
			continue;
		}

		//根据伤害类型，进行效果伤害的处理
		uint8 critflag = 0;  //0普通1暴击2抵挡
		uint8 hpflag = 0;
		int32 nHurtHp = this->calEffectDamage(cAttackObj, cBlowPtr, damageType, strEffectPower, critflag, hpflag, affectdistance);

		//校验三围差攻击加成buff
		uint8 weiType = 0;
		uint16 weiRatio = 0;
		uint32 weiFixValue = 0;
		if( nHurtHp > 0 && cAttackObj->isHave3WeiDBuff(weiType, weiRatio, weiFixValue) )
		{
			int32 nWeiD = cAttackObj->getAttr((uint32)weiType) - cBlowPtr->getAttr((uint32)weiType);
			if( nWeiD < 0 )
				nWeiD = 0;

			int32 nWeiDHurt = nWeiD * weiRatio + weiFixValue;

			nHurtHp += nWeiDHurt;
			if( nHurtHp < 0 )
				nHurtHp = 0;
		}

		//判断是否有抵挡
		bool bblock = false;
		if (nHurtHp > 0 && cAttackObj->getAvatarType() != AVATAR_TYPE_DRAGON && 
			cAttackObj->getMonsterType() != MONSTER_TYPE_WORLD_BOSS  && //世界boss免疫抵挡
			cAttackObj->getMonsterType() != MONSTER_TYPE_MONTH_INS_BOSS  && //三国无双boss免疫抵挡
			damageType != EFFECT_HURT_TYPE_MUST_KILL) 
		{
			uint32 blockattr = cBlowPtr->getAttr(ATTR_Resist);
			uint8 level = cBlowPtr->getLevel();
			uint32 nrand = CRandom::rand(10000);
			if (nrand < blockattr * 10000 / (10000+ level * g_pConsummer->getCambatRate(HURTCAL_PRAM_BLOCK)))
			{
				bblock = true;
				nblockhp += nHurtHp;
				cUint.m_nFlag = 2; //0普通1暴击2抵挡
			}
		}
        
		//如果不是抵挡，则被攻击方血量处理
		if (!bblock && nHurtHp != 0)
		{
			CHurtValues cInf;
			cInf.m_nType = HURT_TYPE_HP;
			int32 nTotHP = cBlowPtr->getFinalHurt(nHurtHp);
			if (nTotHP > 0)
			{
			    totblowhurt += nTotHP;
				nHurtHp = nTotHP;
			}

			cInf.m_nValues = nTotHP * -1;
			cUint.m_nFlag = critflag;   //0普通1暴击2抵挡
			cUint.m_vecAttr.push_back(cInf);
			cBlowPtr->changeHp(cInf.m_nValues);
		}
       
		//必杀的特殊处理
		if (damageType == EFFECT_HURT_TYPE_MUST_KILL)
		{
			cWave.m_vecBlower.push_back(cUint);
			continue;
		}
		
		//如果是拥有禁疗的BUFF，需要发给客户端处理
		if (hpflag == 1)
		{
			CHurtValues cInf;
			cInf.m_nType = HURT_TYPE_HP;
			cInf.m_nValues = 0;
			cUint.m_nFlag = critflag;   //0普通1暴击2抵挡
			cUint.m_vecAttr.push_back(cInf);
		}

		//怒气处理 (需要排除受击不增怒的情况) 
		if ( (addEn > 0 && !cBlowPtr->isHaveEnAddImmBuff() ) || 
			(addEn < 0 && ( cAttackObj->isHaveEnForceDelBuff() || !cBlowPtr->isHaveEnDelImmBuff())) )
		{
			CHurtValues cInf;
			cInf.m_nType = HURT_TYPE_EN;
			cInf.m_nValues = addEn;
			cUint.m_vecAttr.push_back(cInf);
			cBlowPtr->changeEn(cInf.m_nValues);
		}
        
		//技能效果
		int32  blowhurt = this->calEffectResult(cAttackObj, cBlowPtr, effectType, effectValues, nHurtHp, cUint, cAttckUint, battackflag);
		totblowhurt += blowhurt;
        
		//存放受击者的信息
		cWave.m_vecBlower.push_back(cUint);
		if (battackflag)
		    cWave.m_vecBlower.push_back(cAttckUint);

		//判断受影响者是否还存活,并且还是被选中的对象
		if (!cBlowPtr->isDie() && cBlowPtr->getGrid() == attack_grid && affectdistance == 1 && effectTarget == 2)
		{
			if (cBlowPtr->getProfession() != PROFESSION_WUNIANG && cBlowPtr->getProfession() != PROFESSION_YISHI && 
				cAttackObj->getAvatarType() != AVATAR_TYPE_DRAGON && !cAttackObj->isDie() && !cAttackObj->isHaveDevineShield()
				&& cAttackObj->getHp() > nblockhp)
			{
				this->beatbackProc(cAttackObj, cBlowPtr, cWave.m_vecBackUint, cWave);
			}
		}
	}
    
	//出手者结束处理
	this->attackFinalProc(cAttackObj, nblockhp, totblowhurt,cWave);
    
	return;
}

/*
*@出手者最后处理
* blockHp - 抵挡雪量 totblowhurt - 敌方总伤害血量
*/
void CCombatMgr::attackFinalProc(CFightObjPtr cAttackPtr, int32 nblockhp, int32 totblowhurt, CAttackWave &cWave)
{
	//出手者处理（boss免疫抵挡）
	if (cAttackPtr->getAvatarType() != AVATAR_TYPE_DRAGON)
	{   
		//抵挡的伤害给攻击者
		if (nblockhp != 0 && !cAttackPtr->isHaveDevineShield() && 
			(cAttackPtr->getMonsterType() != MONSTER_TYPE_WORLD_BOSS) &&
			(cAttackPtr->getMonsterType() != MONSTER_TYPE_MONTH_INS_BOSS))
		{
			int32 totblockhp = cAttackPtr->getFinalHurt(nblockhp);
			cAttackPtr->changeHp(totblockhp*(-1));
			cWave.m_nBlockHp = totblockhp * (-1);
		}

		//判断是否拥有吸血的BUFF
		if (cAttackPtr->isHaveSuckBloodBuff() && cWave.m_nTarget == EFFECT_TARGET_ENEMY && totblowhurt > 0)
		{
			//出手者吸血处理
			uint32 suckblood = cAttackPtr->getSuckBlood(totblowhurt);
			if (suckblood > 0)
			{
				cAttackPtr->changeHp(suckblood);
				CBlowUint cUint;
				cUint.m_nFightID = cAttackPtr->getAvatarID();
				cUint.m_nFlag = 0;
				CHurtValues cHurtValues;
				cHurtValues.m_nType = HURT_TYPE_HP;
				cHurtValues.m_nValues = suckblood;
				cUint.m_vecAttr.push_back(cHurtValues);
				cWave.m_vecBlower.push_back(cUint);

			}
		}
	}

	return;
}

/*
*@反击信息处理
* 
*/
int CCombatMgr::beatbackProc(CFightObjPtr cAttackObj, CFightObjPtr cBlowPtr, std::vector<CAttckBackUint> &vecBackUint,CAttackWave &cWave)
{
	//判断是否产生反击
	uint32 nrand = CRandom::rand(10000);
	int32 beathp = 0;
	if (nrand < (uint32)cBlowPtr->getAttr(ATTR_StrikeBack))
	{
		beathp = this->beatBack(cBlowPtr, cAttackObj);
		if (beathp > 0)
		{
			CAttckBackUint cbackUint;
			cbackUint.m_nFightID = cBlowPtr->getAvatarID();
			int32 tothp = cAttackObj->getFinalHurt(beathp);
			cbackUint.m_nHp = tothp * -1;
			vecBackUint.push_back(cbackUint);
			cAttackObj->changeHp(cbackUint.m_nHp);
		}
	}

	//判断是否拥有吸血的BUFF
	if (cBlowPtr->isHaveSuckBloodBuff() && beathp > 0)
	{
		//出手者吸血处理
		uint32 suckblood = cBlowPtr->getSuckBlood(beathp);
		if (suckblood > 0)
		{
			cBlowPtr->changeHp(suckblood);
			CBlowUint cUint;
			cUint.m_nFightID = cBlowPtr->getAvatarID();
			cUint.m_nFlag = 0;
			CHurtValues cHurtValues;
			cHurtValues.m_nType = HURT_TYPE_HP;
			cHurtValues.m_nValues = suckblood;
			cUint.m_vecAttr.push_back(cHurtValues);
			cWave.m_vecBlower.push_back(cUint);
		}
	}

	return beathp;
}


/*
*@反击处理
*/
int32 CCombatMgr::beatBack(CFightObjPtr cAttackPtr, CFightObjPtr cBlowPtr)
{
	CLSkillData cSkill;
	cAttackPtr->getNSkill(cSkill);

	//获取技能信息
	std::vector<uint32> vecEffectID;
	g_pConsummer->getSkillEffect(cSkill.m_nSkillID, vecEffectID);
	if (vecEffectID.size() == 0)
		return 0;

	uint32 idEffectID = vecEffectID[0];
	const Json::Value &effectConf = g_pConsummer->effectConfig(idEffectID);
	if (effectConf == Json::nullValue)
		return 0;

	uint8 damage_type = static_cast<uint8>(effectConf["damage_type"].asUInt());
	if (damage_type != EFFECT_HURT_TYPE_PHSIC && damage_type != EFFECT_HURT_TYPE_MAGIC)
		return 0;

	//获取技能威力信息
	std::string strPower = effectConf["effect_power"].asString();
	uint32 hurtvalues = 0;
	double hurtratio = 0.0;
	sscanf(strPower.c_str(), "%d:%lf", &hurtvalues, &hurtratio);
	uint8 nCritFlag = 0;

	uint8 skill_dist_type = static_cast<uint8>(effectConf["affect_distance"].asUInt());
	return this->calhurt(cAttackPtr, hurtvalues, hurtratio, damage_type, skill_dist_type, 1, cBlowPtr, nCritFlag);
}

/*
*@阵型成员战斗后剩余兵力
* 
*/
uint32 CCombatMgr::formationSurplusHp(std::vector<CFormatUint> &vecFormat, std::map<OBJID, CFightObjPtr> &mapBlowObj)
{
	uint32 totHp = 0;
	for (size_t i = 0; i < vecFormat.size(); i++)
	{
		CFormatUint &cUint = vecFormat[i];
		if (cUint.m_nGrid == 16)
			continue;
		std::map<OBJID, CFightObjPtr>::iterator iter = mapBlowObj.find(cUint.m_nFightId);
		if (iter != mapBlowObj.end())
		{
			totHp += iter->second->getHp();
		}
	}

	return totHp;
}
void  CCombatMgr::avatarOutFight(std::vector<CFormatUint> &vecFormat, std::map<OBJID, CFightObjPtr> &mapBlowObj)
{
	for (size_t i = 0; i < vecFormat.size(); i++)
	{
		CFormatUint &cUint = vecFormat[i];
		if (cUint.m_nGrid == 16)
			continue;
		std::map<OBJID, CFightObjPtr>::iterator iter = mapBlowObj.find(cUint.m_nFightId);
		if (iter == mapBlowObj.end())
			continue;
		CFightObjPtr cAvatarPtr = iter->second;
        //清空战斗的BUFF
		cAvatarPtr->clearSenceBuff();
	}
	return;
}

/*
*@阵型成员初始化，并返回总速度
*/
void CCombatMgr::formationObjInit(std::vector<CFormatUint> &vecFormat, std::map<OBJID, CFightObjPtr> &mapBlowObj, uint32 &totSpeed, uint32 &totHp,uint8 combattype)
{
	totSpeed = 0;
	totHp = 0;

	uint8 dragonLvl = 1; //灵龙等级

	for (size_t i = 0; i < vecFormat.size(); i++)
	{
		CFormatUint &cUint = vecFormat[i];
		if (cUint.m_nGrid == 16) //灵龙
		{
			std::map<OBJID, CFightObjPtr>::iterator iter = mapBlowObj.find(cUint.m_nFightId);
			if (iter != mapBlowObj.end())
			{
				CFightObjPtr pDragon = iter->second;
				dragonLvl = pDragon->getLevel();
			}

			continue;
		}

		std::map<OBJID, CFightObjPtr>::iterator iter = mapBlowObj.find(cUint.m_nFightId);
	    if (iter == mapBlowObj.end())
			continue;

		CFightObjPtr cAvatarPtr = iter->second;
		cAvatarPtr->setGrid(cUint.m_nGrid);
        
		//如果是怪物，需要初始化起始BUFF
		if (cAvatarPtr->getAvatarType() == AVATAR_TYPE_MONSTER)
			cAvatarPtr->addInitBuff();

		if (cAvatarPtr->getAvatarType() == AVATAR_TYPE_BUDDY)
			cAvatarPtr->setDragonLevel(dragonLvl);

		cAvatarPtr->makeFinal(ATTR_RANGE_FIGHT_SCENCE);
		uint32 hp = cAvatarPtr->getAttr(ATTR_Hp);
		cAvatarPtr->setHp(hp);
		cAvatarPtr->setMaxHP(hp);
		uint32 en = cAvatarPtr->getAttr(ATTR_En);
		cAvatarPtr->setEn(en);

		totSpeed += cAvatarPtr->getAttr(ATTR_Speed);

		//宝物副本中需保存上次战斗后武将血量
		//护符副本中须保存上次战斗后怪物血量
		//多人副本中需保存上次战斗后的武将或者怪物血量
		if( cUint.m_nCurBlood != cUint.m_nMaxBlood )
		{
			cAvatarPtr->setHp(cUint.m_nCurBlood);
			totHp += cUint.m_nCurBlood;
		}
		else
			totHp += cAvatarPtr->getAttr(ATTR_Hp);
		if (combattype == FIGHT_TYPE_WORLD_CITY || combattype == FIGHT_TYPE_WORLD_PICK || combattype == FIGHT_TYPE_WORLD_DENSE)
		{
			uint32 curen = cUint.m_nCurEn;
			cAvatarPtr->setEn(curen);
		}
	}

	for (size_t i = 0; i < vecFormat.size(); i++)
	{
		CFormatUint &cUint = vecFormat[i];
		if (cUint.m_nGrid == 16)
		{
			//灵龙的速度等于上阵人员的速度之和
			cUint.m_nCurSpeed = totSpeed;
			break;
		}
	}

	return;
}

/*
*@计算效果的伤害
*/
int32 CCombatMgr::calEffectDamage(CFightObjPtr cAttackObj,  CFightObjPtr cBlowPtr, uint8 damagetype, std::string strEffectPower,uint8 &nCrigFlag, uint8 &nHpFlag, uint8 nSkillDistType)
{
	nCrigFlag = 0;
	nHpFlag = 0;
	//根据伤害类型，进行相应的处理
	int32 nHurtHp = 0;
	switch(damagetype)
	{
	case EFFECT_HURT_TYPE_PHSIC: //物理伤害
		{
			uint32 hurtvalues = 0;
			double hurtratio = 0.0;
			sscanf(strEffectPower.c_str(), "%d:%lf", &hurtvalues, &hurtratio);
			nHurtHp = this->calhurt(cAttackObj, hurtvalues, hurtratio, 0, nSkillDistType, 0, cBlowPtr, nCrigFlag);
		}
		break;
	case EFFECT_HURT_TYPE_MAGIC: //魔法伤害
		{
			uint32 hurtvalues = 0;
			double hurtratio = 0.0;
			sscanf(strEffectPower.c_str(), "%d:%lf", &hurtvalues, &hurtratio);
			nHurtHp = this->calhurt(cAttackObj, hurtvalues, hurtratio, 1, nSkillDistType, 0, cBlowPtr, nCrigFlag);
		}
		break;
	case EFFECT_HURT_TYPE_ATTACK_RATIO: //攻击 * 系数
		{
			//获取出手者的攻击力
			double attackvalus = (double)cAttackObj->getAttr(ATTR_Damage);
			//背水效果（失血加buff）
			double curHP = (double)cAttackObj->getHp();                              //当前兵力
			double maxHP = (double)cAttackObj->getAttr(ATTR_Hp);                     //兵力上限
			attackvalus += attackvalus * calLoseHpAddAttrRatio(curHP, maxHP, cAttackObj->getAttr(ATTR_LoseHpAddHurt)) / 10000.0;
			double hurtratio = atof(strEffectPower.c_str());
			uint32 nrate = uint32(hurtratio * 10000);
			nHurtHp = (uint32)(attackvalus * nrate / 10000 );
		}
		break;
	case EFFECT_HURT_TYPE_PERCENT: //百分比
		{
			uint32 attackvalus;
			if( cBlowPtr->getMonsterType() == MONSTER_TYPE_WORLD_BOSS || cBlowPtr->getMonsterType() == MONSTER_TYPE_MONTH_INS_BOSS )
				attackvalus = cAttackObj->getAttr(ATTR_Hp);
			else
				attackvalus = cBlowPtr->getAttr(ATTR_Hp);
			double dratio = atof(strEffectPower.data());
			uint32 nrate = uint32(dratio * 10000);
			nHurtHp = (attackvalus * nrate / 10000 );	
		}
		break;
	case EFFECT_HURT_TYPE_FIX_VALUES: //固定值
		{
			nHurtHp = atoi(strEffectPower.data());
		}
		break;
	case EFFECT_HURT_TYPE_FIX_RATE: //固定伤害系数
		{
			//获取防守者的攻击力
			int32 maxhp;
			if( cBlowPtr->getMonsterType() == MONSTER_TYPE_WORLD_BOSS || cBlowPtr->getMonsterType() == MONSTER_TYPE_MONTH_INS_BOSS )
				maxhp = (int)cAttackObj->getMaxHP();
			else
				maxhp = (int)cBlowPtr->getMaxHP();
			int32 hurtvalues = 0;
			double hurtratio = 0.0;
			sscanf(strEffectPower.c_str(), "%lf:%d", &hurtratio, &hurtvalues);
			int32 nrate = int32(hurtratio * 10000);
			nHurtHp = (maxhp * nrate / 10000 ) + hurtvalues;
		}
		break;
	case EFFECT_HURT_TYPE_DRAGON_FIX: //灵龙固定伤害
		{
			//配置“x:y:z”,伤害效果为目标兵力上限*x+灵龙等级*y+z
			double x = 0;
			int32 y = 0;
			int32 z = 0;
			sscanf(strEffectPower.c_str(), "%lf:%d:%d", &x, &y,&z);
			if( cBlowPtr->getMonsterType() == MONSTER_TYPE_WORLD_BOSS || cBlowPtr->getMonsterType() == MONSTER_TYPE_MONTH_INS_BOSS )
				nHurtHp = (int32)(0 * x + cAttackObj->getLevel() * y + z);
			else
				nHurtHp = (int32)(cBlowPtr->getMaxHP() * x + cAttackObj->getLevel() * y + z);
		}
		break;
	case EFFECT_HURT_TYPE_MAGIC_HP: //x、y分别表示附加攻击力和技能威力，z表示兵力上限的万分比
		{
			uint32 hurtvalues = 0;
			double hurtratio = 0.0;
			uint32 hpratio = 0;
			sscanf(strEffectPower.c_str(), "%d:%lf:%d", &hurtvalues, &hurtratio, &hpratio);
			nHurtHp = this->calhurt(cAttackObj, hurtvalues, hurtratio, 1, nSkillDistType, 0, cBlowPtr, nCrigFlag);

			if( cBlowPtr->getMonsterType() == MONSTER_TYPE_WORLD_BOSS || cBlowPtr->getMonsterType() == MONSTER_TYPE_MONTH_INS_BOSS )
				nHurtHp += cAttackObj->getAttr(ATTR_Hp) * hpratio / 10000;
			else
				nHurtHp += cBlowPtr->getAttr(ATTR_Hp) * hpratio / 10000;
		}
		break;
	case EFFECT_HURT_TYPE_MUST_KILL: //必杀
		{
			nHurtHp = cBlowPtr->getMaxHP() + 99999;
			return nHurtHp;
		}
		break;
	default:
		break;
	}

	//如果是加血的，并且是承受者有禁疗BUFF
	if (nHurtHp < 0 && cBlowPtr->isHaveHPAddImmBuff())
	{
		nHurtHp = 0;
		nHpFlag = 1;
	}
    
	//如果是扣血的，并且(拥有圣盾，且攻击者不是世界boss)
	if (nHurtHp > 0 && (cBlowPtr->isHaveDevineShield() && 
		cAttackObj->getMonsterType() != MONSTER_TYPE_WORLD_BOSS) &&
		cAttackObj->getMonsterType() != MONSTER_TYPE_MONTH_INS_BOSS )
	{
		nHurtHp = 0;
		nHpFlag = 1;
	}

	return nHurtHp;
}

/*
*@计算效果的直接效果
* 
*/
int32 CCombatMgr::calEffectResult(CFightObjPtr cAttackObj,  CFightObjPtr cBlowPtr, uint16 effectType, uint32 effectValues, int32 nHurtHp,
					 CBlowUint &cUint, CBlowUint &cAttckUint, bool &battackflag)
{
	int32 blowhurthp = 0;
	//技能效果
	switch (effectType)
	{
	case RESULT_RECOVER_ATTACK_RATIO: //攻击 * 系数
		{
			if (!cBlowPtr->isHaveHPAddImmBuff())
			{
				uint32 attackvalus = cAttackObj->getAttr(ATTR_Damage);

				CHurtValues cInf;
				cInf.m_nType = HURT_TYPE_HP;
				cInf.m_nValues = attackvalus * effectValues / 10000 * cBlowPtr->getHpAddRate()/10000;
				cUint.m_vecAttr.push_back(cInf);
				cBlowPtr->changeHp(cInf.m_nValues);
				blowhurthp = abs(cInf.m_nValues);
			}
			else
			{
				CHurtValues cInf;
				cInf.m_nType = HURT_TYPE_HP;
				cInf.m_nValues = 0;
				cUint.m_vecAttr.push_back(cInf);
			}
		}
		break;
	case RESULT_RECOVER_PERCENT: //百分比
		{
			if (!cBlowPtr->isHaveHPAddImmBuff())
			{
				uint32 curhp = cBlowPtr->getMaxHP();

				CHurtValues cInf;
				cInf.m_nType = HURT_TYPE_HP;
				cInf.m_nValues = curhp * effectValues / 10000 * cBlowPtr->getHpAddRate()/10000;
				cUint.m_vecAttr.push_back(cInf);
				cBlowPtr->changeHp(cInf.m_nValues);
				blowhurthp = abs(cInf.m_nValues);
			}
			else
			{
				CHurtValues cInf;
				cInf.m_nType = HURT_TYPE_HP;
				cInf.m_nValues = 0;
				cUint.m_vecAttr.push_back(cInf);
			}
		}
		break;
	case RESULT_RECOVER_FIX_VALUES: //固定值
		{
			if (!cBlowPtr->isHaveHPAddImmBuff() && !cBlowPtr->isHaveDevineShield())
			{
				uint32 hp = effectValues * cBlowPtr->getHpAddRate()/10000;
				CHurtValues cInf;
				cInf.m_nType = HURT_TYPE_HP;
				cInf.m_nValues = hp;
				cUint.m_vecAttr.push_back(cInf);
				cBlowPtr->changeHp(cInf.m_nValues);
				blowhurthp = abs(cInf.m_nValues);
			}
			else  if (cBlowPtr->isHaveHPAddImmBuff())
			{
				CHurtValues cInf;
				cInf.m_nType = HURT_TYPE_HP;
				cInf.m_nValues = 0;
				cUint.m_vecAttr.push_back(cInf);
			}
		}
		break;
	case RESULT_EN_DRAW: //吸取怒气（目标单体）
		{
			if (cAttackObj->isHaveEnForceDelBuff() || !cBlowPtr->isHaveEnDelImmBuff())
			{
				int32 natten = cBlowPtr->getEn();
				CHurtValues cInf;
				cInf.m_nType = HURT_TYPE_EN;
				cInf.m_nValues = natten * (-1);
				cUint.m_vecAttr.push_back(cInf);
				cBlowPtr->changeEn(cInf.m_nValues);

				//给出手者增加怒气
				cAttckUint.m_nFightID = cAttackObj->getAvatarID();
				cAttckUint.m_nFlag = 0;
				cInf.m_nValues = natten;
				cAttckUint.m_vecAttr.push_back(cInf);
				battackflag = true;
				cAttackObj->changeEn(cInf.m_nValues);
			}
		}
		break;
	case RESULT_EN_FULL: //鼓舞至满怒
		{
			if (!cBlowPtr->isHaveEnAddImmBuff())
			{
				uint32 nMaxEn = cBlowPtr->getMaxEn();
				CHurtValues cInf;
				cInf.m_nType = HURT_TYPE_EN;
				cInf.m_nValues = nMaxEn;
				cUint.m_vecAttr.push_back(cInf);
				cBlowPtr->changeEn(cInf.m_nValues);
			}
		}
		break;
	case RESULT_EN_CLEAN: //清空怒气
		{
			if (cAttackObj->isHaveEnForceDelBuff() || !cBlowPtr->isHaveEnDelImmBuff())
			{
				int32 nMaxEn = cBlowPtr->getMaxEn();
				CHurtValues cInf;
				cInf.m_nType = HURT_TYPE_EN;
				cInf.m_nValues = nMaxEn * -1;
				cUint.m_vecAttr.push_back(cInf);
				cBlowPtr->changeEn(cInf.m_nValues);
			}
		}
		break;
	case RESULT_HP_HURT_RATIO: //伤害结果*系数
		{   
			//给出手者增加血量
			if (nHurtHp != 0 && !cAttackObj->isHaveHPAddImmBuff())
			{
				uint32 nrate = effectValues;
				CHurtValues cInf;
				cAttckUint.m_nFightID = cAttackObj->getAvatarID();
				cAttckUint.m_nFlag = 0;
				cInf.m_nType = HURT_TYPE_HP;
				cInf.m_nValues = abs(nHurtHp) * nrate / 10000 * cBlowPtr->getHpAddRate()/10000;
				cAttckUint.m_vecAttr.push_back(cInf);
				battackflag = true;
				cAttackObj->changeHp(cInf.m_nValues);
			}
			else if (nHurtHp != 0 && cAttackObj->isHaveHPAddImmBuff())
			{
				CHurtValues cInf;
				cAttckUint.m_nFightID = cAttackObj->getAvatarID();
				cAttckUint.m_nFlag = 0;
				cInf.m_nType = HURT_TYPE_HP;
				cInf.m_nValues = 0;
				cAttckUint.m_vecAttr.push_back(cInf);
			}
		}
		break;
	case RESULT_HP_ATTACK_RATIO: //攻击力*系数
		{
			if (!cAttackObj->isHaveHPAddImmBuff())
			{
				//给出手者增加血量
				uint32 attackvalue  = cAttackObj->getAttr(ATTR_Damage);
				uint32 nrate = effectValues;
				CHurtValues cInf;
				cAttckUint.m_nFightID = cAttackObj->getAvatarID();
				cAttckUint.m_nFlag = 0;
				cInf.m_nType = HURT_TYPE_HP;
				cInf.m_nValues = attackvalue * nrate / 10000 * cBlowPtr->getHpAddRate()/10000;
				cAttckUint.m_vecAttr.push_back(cInf);
				battackflag = true;
				cAttackObj->changeHp(cInf.m_nValues);
			}
			else 
			{
				CHurtValues cInf;
				cAttckUint.m_nFightID = cAttackObj->getAvatarID();
				cAttckUint.m_nFlag = 0;
				cInf.m_nType = HURT_TYPE_HP;
				cInf.m_nValues = 0;
				cAttckUint.m_vecAttr.push_back(cInf);
			}
		}
		break;
	case RESULT_HP_FIX_VALUES://固定值
		{
			if (!cAttackObj->isHaveHPAddImmBuff())
			{
				uint32 nhp = effectValues * cBlowPtr->getHpAddRate()/10000;
				CHurtValues cInf;
				cAttckUint.m_nFightID = cAttackObj->getAvatarID();
				cAttckUint.m_nFlag = 0;
				cInf.m_nType = HURT_TYPE_HP;
				cInf.m_nValues = nhp;
				cAttckUint.m_vecAttr.push_back(cInf);
				battackflag = true;
				cAttackObj->changeHp(cInf.m_nValues);
			}
			else
			{
				CHurtValues cInf;
				cAttckUint.m_nFightID = cAttackObj->getAvatarID();
				cAttckUint.m_nFlag = 0;
				cInf.m_nType = HURT_TYPE_HP;
				cInf.m_nValues = 0;
				cAttckUint.m_vecAttr.push_back(cInf);
			}
		}
		break;
	case RESULT_BUFF_DEL_BENEFIT://删除增益
		{
			uint32 radomvalues = CRandom::rand(10000);
			if (radomvalues <= effectValues)
			{
				cBlowPtr->delBuff(cUint.m_vecDelBuff, 0);
				cBlowPtr->updateAttr(ATTRTYPE_BUFF, true);
			}
		}
		break;
	case RESULT_BUFF_DEL_LOSS: //删除减益
		{
			uint32 radomvalues = CRandom::rand(10000);
			if (radomvalues <= effectValues)
			{
				cBlowPtr->delBuff(cUint.m_vecDelBuff, 1);
				cBlowPtr->updateAttr(ATTRTYPE_BUFF, true);
			}
		}
		break;
	default:
		break;
	}

	return blowhurthp;
}


/*
*@获取兵种克制的数据
*/
void CCombatMgr::getProfRestrain(uint8 profession, uint8 prof_rest, double &initRestVal, double &growRestVal)
{
	uint32 nprof = (uint32)profession;
	uint32 resprof = (uint32)prof_rest;
	uint32 nKeyVal = nprof * 100 + resprof;

	initRestVal = 0;
	growRestVal = 0;

	std::map<uint32, CProfLevel>::iterator iter = m_mapProfLevel.find(nKeyVal);
	if (iter != m_mapProfLevel.end())
	{
		CProfLevel &cInf = iter->second;
		initRestVal = (double)cInf.m_nInitRestVal;
		growRestVal = (double)cInf.m_nGrowRestVal;
	}
    
	return;
}


/*******************************************************  文件结束 *********************************************************/

