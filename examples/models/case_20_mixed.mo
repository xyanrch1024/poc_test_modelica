model Case20Mixed
  constant Real kb = 1.5;
  parameter Real kp = 2 * kb;
  parameter Integer ki = 2;
  constant Integer ai = 2;
  constant Integer bi = 3;
  parameter Boolean bp = true;
  Real xs(start = -1.5, fixed = true);
  Real ya;
  Real yb;
  Integer ni;
  Boolean bo;
equation
  der(xs) = kp * xs + ki;
  ya = min(abs(xs), 10);
  yb = ya / 2 + 0.25;
  ni = ai * bi + ki;
  bo = bp or (xs > -100);
  annotation(experiment(StartTime = 0, StopTime = 2, Interval = 0.02));
end Case20Mixed;
