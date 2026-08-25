model Case17DivPrec
  parameter Real dA = 2;
  parameter Real dB = 4;
  parameter Real dC = 0.5;
  Real z17(start = 1, fixed = true);
equation
  der(z17) = ((z17 / dA) + (dC * dA)) - dB / (z17 + 2);
  annotation(experiment(StartTime = 0, StopTime = 1, Interval = 0.01));
end Case17DivPrec;
