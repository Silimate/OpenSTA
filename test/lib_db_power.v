module lib_db_power (
  input ck,
  input d,
  input si,
  input se,
  output y
);

  wire q;

  SDFF r1 (
    .D(d),
    .SI(si),
    .SE(se),
    .CK(ck),
    .Q(q)
  );

  BUF u1 (
    .A(q),
    .Y(y)
  );

endmodule
