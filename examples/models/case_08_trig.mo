model Case08Trig
  parameter Real w = 1.5;
  Real a(start = 0, fixed = true);
  Real ph(start = 0, fixed = true);
equation
  der(a) = w * cos(ph);
  der(ph) = w;
  annotation(experiment(StartTime = 0, StopTime = 2, Interval = 0.02));
end Case08Trig;
