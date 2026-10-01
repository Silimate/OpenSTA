// Leaf block: two clock ports, neg-edge capture, cross-clock and
// reconvergent internal paths.
module blk(clka, clkb, a, y, z);
  input clka, clkb, a;
  output y, z;
  wire cka, ckan, ckb, q1, q2, q3, q4, q5, n1, n2, n3, s1, l1, l2, l3, c5;
  CLKBUF cba(.A(clka), .Y(cka));
  INV cbn(.A(cka), .Y(ckan));
  CLKBUF cbb(.A(clkb), .Y(ckb));
  DFF r1(.CK(cka), .D(a), .Q(q1));
  // r1 -> r2 long
  BUF u1(.A(q1), .Y(n1));
  BUF u2(.A(n1), .Y(n2));
  BUF u3(.A(n2), .Y(n3));
  DFF r2(.CK(cka), .D(n3), .Q(q2));
  // r1 -> r3 on the falling edge
  BUF u4(.A(q1), .Y(s1));
  DFF r3(.CK(ckan), .D(s1), .Q(q3));
  // r2 -> r4 across to clkb
  DFF r4(.CK(ckb), .D(q2), .Q(q4));
  BUF u5(.A(q4), .Y(y));
  // r3 -> r5 reconvergent: 0 or 3 buffers into AND
  BUF u6(.A(q3), .Y(l1));
  BUF u7(.A(l1), .Y(l2));
  BUF u8(.A(l2), .Y(l3));
  AND2 g5(.A(q3), .B(l3), .Y(c5));
  DFF r5(.CK(cka), .D(c5), .Q(q5));
  BUF u9(.A(q5), .Y(z));
endmodule
