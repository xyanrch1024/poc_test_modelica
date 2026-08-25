model Case04LinearSource
  parameter Real a = 0.8;
  parameter Real b = 1.5;
  Real y(start = 0, fixed = true);
equation
  der(y) = -a * y + b;
  annotation(experiment(StartTime = 0, StopTime = 4, Interval = 0.04));
end Case04LinearSource;
