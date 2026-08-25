model Cooling
  parameter Real k = 0.5;
  parameter Real Tamb = 20;
  Real T(start = 90, fixed = true);
equation
  der(T) = -k * (T - Tamb);
  annotation(experiment(StartTime = 0, StopTime = 5, Interval = 0.01));
end Cooling;
