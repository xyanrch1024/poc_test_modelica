model Case09ExpLog
  Real g(start = 0, fixed = true);
equation
  der(g) = exp(-g) - 1;
  annotation(experiment(StartTime = 0, StopTime = 3, Interval = 0.03));
end Case09ExpLog;
