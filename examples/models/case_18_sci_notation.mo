model Case18SciNotation
  parameter Real small = 1e-3;
  parameter Real big = 2.5e2;
  Real y18(start = 100, fixed = true);
equation
  der(y18) = -small * big * y18 / 25;
  annotation(experiment(StartTime = 0, StopTime = 2, Interval = 0.02));
end Case18SciNotation;
