model Case01Decay
  parameter Real k = 0.5;
  Real x(start = 2, fixed = true);
equation
  der(x) = -k * x;
  annotation(experiment(StartTime = 0, StopTime = 2, Interval = 0.02));
end Case01Decay;
