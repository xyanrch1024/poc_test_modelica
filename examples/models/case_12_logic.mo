model Case12Logic
  Real x2(start = 2, fixed = true);
  Real y2;
  Boolean f2;
equation
  der(x2) = -x2;
  y2 = x2 * 0.5;
  f2 = not (x2 > 1) or (y2 <= 3) and (x2 <> 0);
  annotation(experiment(StartTime = 0, StopTime = 2, Interval = 0.05));
end Case12Logic;
