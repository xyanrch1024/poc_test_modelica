model Case07AbsMin
  parameter Real cap = 6;
  parameter Real rate = 0.4;
  Real m(start = 10, fixed = true);
  Real clipped;
equation
  der(m) = -rate * min(abs(m), cap);
  clipped = min(abs(m), cap);
  annotation(experiment(StartTime = 0, StopTime = 2, Interval = 0.02));
end Case07AbsMin;
