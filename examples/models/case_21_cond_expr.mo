model Case21CondExpr
  parameter Real y0 = 1.0;
  parameter Real slope = 2.0;
  Real y;
  Real x(start = 0, fixed = true);
equation
  y = if x > 1 then y0 + slope * (x - 1) else y0 + x;
  der(x) = 1;
  annotation(experiment(StartTime = 0, StopTime = 3, Interval = 0.01));
end Case21CondExpr;