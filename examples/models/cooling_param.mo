model CoolingParam
  parameter Real k = 0.8;
  parameter Real Tamb = 25;
  Real T(start = 100, fixed = true);
equation
  der(T) = -k * (T - Tamb);
  annotation(experiment(StartTime = 0, StopTime = 4, Interval = 0.02));
end CoolingParam;
