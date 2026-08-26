model RoomHeating
  // ---- 参数 ----
  parameter Real CRoom = 60360;   // 房间空气热容 J/K（50m³）
  parameter Real kVent = 24;      // 渗风换热 W/K
  parameter Real TOut = -5;       // 室外温度 °C
  parameter Real QHeater = 500;   // 加热器恒定功率 W
  parameter Real TInt = 200;      // 内部得热 W

  // ---- 状态 ----
  Real TRoom(start = 15, fixed = true, unit = "degC");

equation
  // 热平衡：加热 + 内部得热 − 渗风损失 = 升温速度
  der(TRoom) = (QHeater + TInt - kVent * (TRoom - TOut)) / CRoom;

  annotation(experiment(StartTime = 0, StopTime = 10800, Interval = 30));
end RoomHeating;
