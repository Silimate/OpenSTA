module power_leakage_default (
  input a,
  output z1,
  output z2
);

  PARTIAL_DEFAULT u1 (
    .A(a),
    .Z(z1)
  );

  PARTIAL_CELL_LEAKAGE u2 (
    .A(a),
    .Z(z2)
  );

endmodule
