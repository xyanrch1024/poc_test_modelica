model Case03Oscillator
  parameter Real w = 2.5;
  Real x(start = 1, fixed = true);
  Real v(start = 0, fixed = true);
equation
  der(x) = v;
  der(v) = -w * w * x;
  annotation(experiment(StartTime = 0, StopTime = 2, Interval = 0.01));
end Case03Oscillator;
