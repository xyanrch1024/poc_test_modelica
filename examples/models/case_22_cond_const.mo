model Case22CondConst
  parameter Real x = 5;
  Real y;
  Real z;
equation
  y = if x > 2 then x * 1.5 else x;
  z = if x > 0 then y else 1;
  annotation(experiment(StartTime = 0, StopTime = 1, Interval = 0.1));
end Case22CondConst;