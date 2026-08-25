model Case06BoolInt
  parameter Boolean flagIn = true;
  parameter Integer k2 = 2;
  Real x(start = 0, fixed = true);
  Boolean b;
  Integer n;
equation
  der(x) = 0.5;
  b = (x >= 0.25) and flagIn;
  n = 3 + 4 * k2;
  annotation(experiment(StartTime = 0, StopTime = 1, Interval = 0.05));
end Case06BoolInt;
