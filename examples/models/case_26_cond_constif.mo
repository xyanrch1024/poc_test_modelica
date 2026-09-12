model Case26CondConstIf
  Real v;
  constant Boolean flag = true;
  Real s(start = 0, fixed = true);
equation
  if flag then
    v = 2;
  end if;
  der(s) = 0.5 * v;
  annotation(experiment(StartTime = 0, StopTime = 2, Interval = 0.25));
end Case26CondConstIf;