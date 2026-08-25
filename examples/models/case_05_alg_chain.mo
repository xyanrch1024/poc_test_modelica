model AlgChain
  Real s(start = 1, fixed = true);
  Real u;
  Real v;
  Real z;
equation
  der(s) = z;
  z = u * v + 1;
  u = s * 2;
  v = 3;
  annotation(experiment(StartTime = 0, StopTime = 1, Interval = 0.1));
end AlgChain;
