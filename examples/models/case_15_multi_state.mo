model Case15MultiState
  parameter Real r = 0.7;
  Real a1(start = 1, fixed = true);
  Real a2(start = 2, fixed = true);
  Real a3(start = 3, fixed = true);
  Real a4(start = 4, fixed = true);
  Real a5(start = 5, fixed = true);
equation
  der(a1) = -r * a1;
  der(a2) = -r * a2;
  der(a3) = -r * a3;
  der(a4) = -r * a4;
  der(a5) = -r * a5;
  annotation(experiment(StartTime = 0, StopTime = 1, Interval = 0.01));
end Case15MultiState;
