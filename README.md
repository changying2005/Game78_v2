# 灵脉1998 (LingMai 1998)

第三人称动作冒险游戏，使用 Unreal Engine 5 开发。玩家操控自定义角色探索神秘关卡、收集资源、击败敌人，逐步解锁更高难度内容与隐藏剧情。

<div align="center">
  <img src="https://github.com/user-attachments/assets/0281803b-9fb2-496a-b492-e3b8ca28e5a3" alt="游戏主画面 - 主角在神秘科技关卡探索" width="80%">
</div>

## 核心玩法

- **探索**：广阔关卡包含隐藏路径、秘密宝箱（稀有装备/资源）、可破坏障碍物、可利用的环境陷阱，鼓励玩家深入搜寻。
- **战斗**：敌人拥有专属AI，玩家结合普通攻击 + 技能组合打击。战斗强调走位、时机把握（躲避敌人技能、寻找攻击间隙）。
- **成长系统**：通过收集资源提升角色实力，解锁新技能、装备与更高难度关卡。
- **存档机制**：关卡内固定存档点，死亡后从最近存档复活，保留进度。

<div align="center">
  <img src="https://github.com/user-attachments/assets/04020422-37ed-4df0-a904-bc47db197cac" alt="玩家进行三连击与闪避战斗" width="70%">
  <p><em>可控三连击组合&走位闪避，突出打击反馈与节奏感</em></p>
</div>

## 技术亮点（Blueprints + Niagara + AI）

- **可控三连击系统**：利用蓝图接口与事件分发器实现多段取消的三连击，打击反馈多样化（不同段位不同特效/音效）。
- **翻滚闪避特效**：使用 Niagara 系统制作科技感十足的运行轨迹、残影与粒子效果。
- **高级移动交互**：状态机 + 角色蓝图实现爬墙、落地翻滚、环境实时交互。
- **敌人AI**：行为树（Behavior Tree）+ 黑板键（Blackboard）实现远近距离不同策略：
  - 自动计算玩家方位
  - 多种攻击动画
  - 被击杀特效
  - 智能巡逻 / 追击 / 攻击切换

<div align="center">
  <img src="https://github.com/user-attachments/assets/5815998d-0097-4169-8f53-f1f6d4f26156" alt="Niagara 系统翻滚闪避粒子特效" width="60%">
  <p><em>Niagara 翻滚闪避特效：科技感轨迹、残影与动态粒子，增强动作流畅度</em></p>
</div>

## 截图画廊
<div align="center">
  <img src="https://github.com/user-attachments/assets/5b345e43-77aa-4b88-8798-58e7e3e10059" alt="关卡隐藏路径与宝箱探索" width="70%">

  <p><em>潜行玩法：玩家发现工业区秘密与宝箱，展示关卡设计深度</em></p>
</div>

<div align="center">
  <img src="https://github.com/user-attachments/assets/2bc7e92d-4a48-40b9-8064-e7361b800e52" alt="敌人AI追击与攻击玩家" width="70%">
  <p><em>关卡展示1</em></p>
</div>

<div align="center">
  <img src="https://github.com/user-attachments/assets/61d40cba-af0c-4941-9897-3b566f635807" alt="角色爬墙与环境交互" width="70%">
  <p><em>关卡展示2</em></p>
</div>

<div align="center">
  <img src="https://github.com/user-attachments/assets/15aa2a0d-306b-4a97-8e63-c1a54413b5e6" alt="敌人被击杀特效与多种攻击动画" width="70%">
  <p><em>被击杀粒子特效，丰富视觉反馈</em></p>
</div>

## 未来计划

- 添加更多敌人类型与 Boss 战
- 完善技能树与装备系统
- 优化性能（Nanite / Lumen 支持）
- 多人合作模式探索
