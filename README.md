# 灵脉1998 (LingMai 1998)

第三人称动作冒险游戏，使用 Unreal Engine 5 开发。玩家操控自定义角色探索神秘关卡、收集资源、击败敌人，逐步解锁更高难度内容与隐藏剧情。

<img width="2172" height="930" alt="310fc98525ada191242c43d0e2c22d49" src="https://github.com/user-attachments/assets/0281803b-9fb2-496a-b492-e3b8ca28e5a3" />


## 核心玩法

- **探索**：广阔关卡包含隐藏路径、秘密宝箱（稀有装备/资源）、可破坏障碍物、可利用的环境陷阱，鼓励玩家深入搜寻。
- **战斗**：敌人拥有专属AI，玩家结合普通攻击 + 技能组合打击。战斗强调走位、时机把握（躲避敌人技能、寻找攻击间隙）。
- **成长系统**：通过收集资源提升角色实力，解锁新技能、装备与更高难度关卡。
- **存档机制**：关卡内固定存档点，死亡后从最近存档复活，保留进度。

<img width="1305" height="723" alt="e9ce59869010621bd34a3567fc306ada" src="https://github.com/user-attachments/assets/04020422-37ed-4df0-a904-bc47db197cac" />

## 技术亮点（Blueprints + Niagara + AI）

- **可控三连击系统**：利用蓝图接口与事件分发器实现多段取消的三连击，打击反馈多样化（不同段位不同特效/音效）。
- **翻滚闪避特效**：使用 Niagara 系统制作科技感十足的运行轨迹、残影与粒子效果。
- **高级移动交互**：状态机 + 角色蓝图实现爬墙、落地翻滚、环境实时交互。
- **敌人AI**：行为树（Behavior Tree）+ 黑板键（Blackboard）实现远近距离不同策略：
  - 自动计算玩家方位
  - 多种攻击动画
  - 被击杀特效
  - 智能巡逻 / 追击 / 攻击切换

<img width="531" height="267" alt="02349e3e5811a05bb542e660b9546661" src="https://github.com/user-attachments/assets/5815998d-0097-4169-8f53-f1f6d4f26156" />

## 系统要求（开发环境）

- Unreal Engine 5.x（推荐 5.3+）
- Windows 10/11（开发平台）
- 支持蓝图开发 + C++ 扩展（当前主要蓝图实现）

## 如何运行 / 构建

1. Clone 本仓库
2. 用 Unreal Engine Launcher 打开项目（.uproject 文件）
3. 等待 Shader 编译完成
4. Play in Editor 或 Package 项目

（后续可添加打包说明、控制键位表等）

## 截图画廊

<img width="1470" height="716" alt="56a883ae513867cd0e0b68be868463ec" src="https://github.com/user-attachments/assets/447e778f-4cdd-4c5c-a28d-3ba9798dd58b" />
<img width="1461" height="729" alt="ff8e894b45179e5a0a17600ce0d426e9" src="https://github.com/user-attachments/assets/2bc7e92d-4a48-40b9-8064-e7361b800e52" />
<img width="1470" height="720" alt="684030adb661aa3c331649d853f8b187" src="https://github.com/user-attachments/assets/61d40cba-af0c-4941-9897-3b566f635807" />


## 未来计划

- 添加更多敌人类型与 Boss 战
- 完善技能树与装备系统
- 优化性能（Nanite / Lumen 支持）
- 多人合作模式探索
