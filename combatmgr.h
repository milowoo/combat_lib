/******************************************************
 *	    Copyright 2014, by linglong.
 *			All right reserved.
 *功能：战斗处理模块
 *日期：2014-3-28 20:15
 *作者：milo_woo
 ******************************************************/
#ifndef __COMBAT_MGR_
#define __COMBAT_MGR_

#include <framework/daserver.h>
#include <framework/endpoint.h>
#include <util/refshare.h>
#include <common/util/singleton.h>
#include <common/util/tinytimer.h>
#include "fightobj.h"
#include "player.h"
#include "buddy.h"
#include "monster.h"
#include "business/fightdefine.h"
#include <gateway/gwchallengeinstance.h>
#include <business/staticdatadef.h>
#include <business/ibuff.h>
#include <business/iskilleffect.h>

#define IS_MALE(sex)	(1 == (sex))
#define IS_FEMALE(sex)	(2 == (sex))

typedef struct _t_rand_buff
{
	uint16 m_nRate;   //产生BUFF的概率
	uint32 m_nBuffID; //产生的BUFF
}CRandBuff;

//buff 处理后结果
enum EBUFF_PROC_END  
{
	BUFF_PROC_END_CONTINUE   = 0, //继续处理
	BUFF_PROC_END_SILENCE	 = 1, //沉默
	BUFF_PROC_END_VERTIGO	 = 2, //眩晕
};

//攻击信息
typedef struct _t_attack_type
{
    uint8  m_nType;      //0BUFF效果 1技能效果
	int32  m_nBuffID;    //BUFF id
	int32  m_nSkillID;   //技能ID
	uint32 m_nEffectID;  //效果ID
}CAttckType;

typedef struct _t_prof_level
{
	uint32 m_nInitRestVal; //初始克制系数
	uint32 m_nGrowRestVal; //成长克制系数
}CProfLevel;

//战斗仇恨控制管理类
class CCombatMgr : public CSingleton<CCombatMgr>, public CRefShare
{
public:
	CCombatMgr();
	virtual ~CCombatMgr();

	friend class CRefObject<CCombatMgr>;

public:
	void relolutionTable(bool bUpdate);
    
	/*
	*@战斗过程处理
	*mapAvatarPtr 参与对象
	* gwresponse 战斗数据
	*/
	void combatProc(std::map<OBJID, CFightObjPtr> &mapAvatarPtr, CFightData &cFightData, uint8 nCalStar = 1);

private:

	/*
	*@获取承受伤害的位置
	*vecFormation 受击方阵型 mapFightObject 战斗对象 attackgrid --攻击对象位置 attack_type 攻击类型
	*blowgrid 受击对象位置
	*/
	void getBlowGrid(std::vector<CFormatUint> &vecFormation, std::map<OBJID, CFightObjPtr> &mapFightObject,
		             uint8 attackgrid, uint16 attack_type, uint8 &blowgrid);
    
	/*
	*@获取承受伤害的对象
	*vecFormation 受击方阵型 grid --受击对象 rang_type 受击范围 mapFightObject-战斗人员信息
	*mapBlowObj 战斗人员
	*/
	void getBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid, uint16 rang_type,
		            std::map<OBJID, CFightObjPtr> &mapFightObject, 
					std::map<OBJID, CFightObjPtr> &mapBlowObj);
    
	/*
	*@获取承受伤害的对象(自爆兵使用)
	*vecFormation 受击方阵型 
	*mapBlowObj 战斗人员 
	*/
	void getAllFightObj(std::vector<CFormatUint>& vecBlowFormation, 
		               std::vector<CFormatUint>& vecAttFormation,
					   std::map<OBJID, CFightObjPtr> &mapFightObject, 
					   std::map<OBJID, CFightObjPtr> &mapBlowObj, 
					   uint8 &blowgrid);

	/*
	*@计算受击方影响
	*  cAttackObj 出手者 attack_grid 选中目标， mapBlowObject 受击方对象 damagetype --效果类型 rand_buff 随机BUFF 
	*    affect_type 属性增加类型 affect_value 影响值
	*m_vecBlower 受击方结果
	*/
	void calBlowEffect(CFightObjPtr cAttackObj, uint8 attack_grid, std::map<OBJID, CFightObjPtr> &mapBlowObj, 
		               const Json::Value& effectConf, CAttackWave &cWave);

	/**
	* @功能：计算伤害
	* @输入：pattackPtr 攻击方，nskillID 技能伤害，hurtratio 技能伤害系数 nSkillHurtType 伤害类型 pdefendPtr 防守方 attackType 攻击类型 0 普通 1反击
	* @输出：nGritFlag
	* @返回：伤害值
	*/
	uint32 calhurt(CFightObjPtr pattackPtr, uint32 nSkillHurt, double hurtratio, uint8 nSkillHurtType, uint8 nSkillDistType,
		         uint8 attackType, CFightObjPtr pdefendPtr,uint8 &nGritFlag);

	//失血加伤、防系数
	double calLoseHpAddAttrRatio(double dCurHp, double dMaxHp, uint32 nAttrValue); 

private:

	/*
	*@校验阵型是否还有对象存活
	*/
	bool isHavaAlive(std::vector<CFormatUint> &vecFormat, std::map<OBJID, CFightObjPtr> &mapBlowObj);


	/*
	*@重置出手标志
	*/
	void resetFightFlag(std::map<OBJID, CFightObjPtr> &mapBlowObj);

	/*
	*@找出第一个未出手者
	* nRound 回合数
	*/
	CFightObjPtr getFirstNoFightAvatar(uint8 nRound, std::vector<CFormatUint> &vecFormat, std::map<OBJID, CFightObjPtr> &mapBlowObj);

	/*
	*@出战单位一次攻击处理
	*cAvatarPtr 出手者 mapBlowObj 全体出战对象 vecFormat 出手方阵型 vecRecFormat 受击方阵型 fight_count 回合数
	*gwresponse 战斗数据
	*/
	void getAttackRound(CFightObjPtr cAvatarPtr, std::vector<CFormatUint> &vecFormat, std::vector<CFormatUint> &vecRecFormat,
		                  std::map<OBJID, CFightObjPtr> &mapBlowObj, CAttackRound &cAttackRound);

	/*
	*@出战单位的BUFF处理
	*/
    uint8 buffEffectProc(CFightObjPtr cAvatarPtr, std::vector<CBuffEffect> &vecBuffEffect);

	/*
	*@cAvatarPtr 出手者 mapBlowObj 全体出战对象 vecFormat 出手方阵型 vecRecFormat 受击方阵型 
	*cWave 战斗数据
	*/
	bool skillEffectProc(CFightObjPtr cAvatarPtr, uint32 idSkill, uint32 idEffect, std::vector<CFormatUint> &vecFormat, 
		           std::vector<CFormatUint> &vecRecFormat, std::map<OBJID, CFightObjPtr> &mapBlowObj,
				   uint8 skill_obj_choice, uint8 &skill_grid, CAttackWave &cWave, std::vector<uint32> &vecChaseEffectId );

	//阵型中最左边，若存在两个或以上，优先判断前排
	void getLeftMostObjGrid(std::map<OBJID, CFightObjPtr> &mapFightObject, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid);

	//阵型中最右边，若存在两个或以上，优先判断前排
	void getRightMostObjGrid(std::map<OBJID, CFightObjPtr> &mapFightObject, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid);
    
	/*
	*@反击处理
	*/
	int32 beatBack(CFightObjPtr cAttackPtr, CFightObjPtr cBlowPtr);
    
	/*
	*@阵型成员初始化，并返回总速度
	* totSpeed 总速度 totHp 总兵力
	*/
	void formationObjInit(std::vector<CFormatUint> &vecFormat, std::map<OBJID, CFightObjPtr> &mapBlowObj, uint32 &totSpeed, uint32 &totHp,uint8 combattype);
    
	/*
	*@计算效果的伤害
	* nCrigFlag 暴击标志
	*/
	int32 calEffectDamage(CFightObjPtr cAvatarPtr,  CFightObjPtr cBlowPtr, uint8 damagetype, std::string strEffectPower,uint8 &nCrigFlag, uint8 &nHpFlag, uint8 nSkillDistType);

	/*
	*@计算效果的直接效果
	* 
	*/
	int32 calEffectResult(CFightObjPtr cAvatarPtr,  CFightObjPtr cBlowPtr, uint16 effectType, uint32 effectValue, int32 nHurtHp,
		                 CBlowUint &cUint, CBlowUint &cAttckUint, bool &battackflag);

	/*
	*@反击信息处理
	* 
	*/
	int32 beatbackProc(CFightObjPtr cAttackObj, CFightObjPtr cBlowPtr, std::vector<CAttckBackUint> &vecBackUint,CAttackWave &cWave);
    
	/*
	*@成功挑战星级
	*/
	uint8 getSuccessStar(std::vector<CFormatUint> &vecFormat,  std::map<OBJID, CFightObjPtr> &mapBlowObj, uint32 totHp);

	/*
	*@成功挑战星级
	*/
	uint8 getFailStar(std::vector<CFormatUint> &vecFormat,  std::map<OBJID, CFightObjPtr> &mapBlowObj, uint32 totHp);


	/*
	*@阵型成员战斗后剩余兵力
	* 
	*/
	uint32 formationSurplusHp(std::vector<CFormatUint> &vecFormat, std::map<OBJID, CFightObjPtr> &mapBlowObj);

	/*
	*@获取灵龙战斗技能
	* 
	*/
	void getDragonFightSkill(uint8 nRound, std::vector<CLSkillData> &vecSkill, CLSkillData &cSkill);

    /*
	*@脱离战斗处理
	*/
	void avatarOutFight(std::vector<CFormatUint> &vecFormat, std::map<OBJID, CFightObjPtr> &mapBlowObj);
    
	/*
	*@获取兵种克制的数据
	*/
	void getProfRestrain(uint8 profession, uint8 prof_rest, double &initRestVal, double &growRestVal);

	/*
	*@出手者最后处理
	* blockHp - 抵挡雪量 totblowhurt - 地方总伤害血量
	*/
	void attackFinalProc(CFightObjPtr cAttackPtr, int32 blockHp, int32 totblowhurt, CAttackWave &cWave);

	/*
	*@随机获取数据
	*/
	void getrandObj(std::vector<OBJID> &vecObj,std::map<OBJID, CFightObjPtr> &mapFightObject, std::map<OBJID, CFightObjPtr> &mapBlowObj);

	/*
	*@校验格子的玩家是否存活
	*/
	bool isSurvivalGrid(std::vector<CFormatUint> &vecFormat, std::map<OBJID, CFightObjPtr> &mapBlowObj, uint8 grid);

	/*
	*@计算兵种伤害
	*/
	double getProfessionHurt(CFightObjPtr cAttObjPtr, CFightObjPtr cDefObjPtr, double dprofRestHurt);


	double calAttackTypeDemageRate(CFightObjPtr pAttacker, CFightObjPtr pDefender, uint8 nSkillHurtType);

	double calSexDemageRate(CFightObjPtr pAttacker, CFightObjPtr pDefender);

	//计算技能远程、近战产生的伤害加成和减免
	double calDistDemageRate(CFightObjPtr pAttacker, CFightObjPtr pDefender, uint8 nSkillDistType);
		
    
private:
	/************** 获取选择目标 begin *****************/
	//获取最近的对象
	void getNearBlowGrid(uint8 attackgrid, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid);
	//获取最远的对象
	void getFarBlowGrid(uint8 attackgrid, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid);
	//获取血量最低的对象
	void getMinHpBlowGrid(std::map<OBJID, CFightObjPtr> &mapFightObject, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid);
	//获取血量最高的对象
	void getMaxHpBlowGrid(std::map<OBJID, CFightObjPtr> &mapFightObject,std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid);
	//获取怒气最低的对象
	void getMinEnBlowGrid(std::map<OBJID, CFightObjPtr> &mapFightObject, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid);
	//获取怒气最高的对象
	void getMaxEnBlowGrid(std::map<OBJID, CFightObjPtr> &mapFightObject, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid);
	//获取随机的对象
	void getRandBlowGrid(std::map<OBJID, CFightObjPtr> &mapFightObject, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid);
	//获取后方的对象 (后方列距离最小，相同取id小(优先同一行))
	void getBackBlowGrid(uint8 attackgrid, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid);
    //优先选择不满怒的随机单位
	void getNoMaxENGrid(std::map<OBJID, CFightObjPtr> &mapFightObject, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid);

	//选择兵力百分比最低的单位
	void getMinHpRateObjGrid(std::map<OBJID, CFightObjPtr> &mapFightObject, std::vector<CFormatUint> &vecAliveObj, uint8 &blowgrid);
	/*************** 获取选择目标 end ******************/

	/************** 获取选择范围 begin *****************/
	//获取单体的对象
	void getOnlyBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, std::map<OBJID, CFightObjPtr> &mapBlowObj);
	//单列 -- 以自己为目标，纵列(竖)
	void getOneRankBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, std::map<OBJID, CFightObjPtr> &mapBlowObj);
	//单行 -- 以自己为目标，横排
	void getOneLineBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, std::map<OBJID, CFightObjPtr> &mapBlowObj);
	//十字 -- 以自己为目标，十字
	void getCrossBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, std::map<OBJID, CFightObjPtr> &mapBlowObj);
	//两翼
	void getCountBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, std::map<OBJID, CFightObjPtr> &mapBlowObj);
	//群体
	void getAllBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, std::map<OBJID, CFightObjPtr> &mapBlowObj);
	//3*3 -- 以自己为目标，3*3
	void getAroundBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, std::map<OBJID, CFightObjPtr> &mapBlowObj);
	//随机两个目标
	void getRand2BlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, std::map<OBJID, CFightObjPtr> &mapBlowObj);
	//穿刺
	void getTapsBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, std::map<OBJID, CFightObjPtr> &mapBlowObj);
	//波形单位
	void getWaveBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, std::map<OBJID, CFightObjPtr> &mapBlowObj);
	//X型
	void getXBlowObj(std::vector<CFormatUint>& vecFormation, uint8 grid,std::map<OBJID, CFightObjPtr> &mapFightObject, std::map<OBJID, CFightObjPtr> &mapBlowObj);
	/************** 获取选择范围 end *******************/

	void adjhurtrate(double &dRate)
	{
		if (dRate > 2.0)
			dRate = 2.0;
		else if (dRate < 0.2)
			dRate = 0.2;
	}
	



public:
	void incRef()
	{
		CRefShare::incRef();
	}

	bool decRef()
	{
		return CRefShare::decRef();
	}

private:
	size_t m_nMaxFightCount;  //最大战斗回合
	std::map<uint32, CProfLevel> m_mapProfLevel;	
};

#endif
