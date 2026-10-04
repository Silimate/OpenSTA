// Latches (positive, negative, latch to latch, input to latch to output,
// latch to output, resettable, inferred from timing arcs) and a divide by 2
// generated clock with crossings.
module latch_blk(clk, a, b, y, z, w);
  input clk, a, b;
  output y, z, w;
  wire ck, q1, n1, n2, n3, lq1, m1, q2, m2, lq2, a1, lq3, gclk, divn, gck, q3, n9, q4, q5,
    lq4, m4, q6, lq5, q7, rn1, rn2, rn3;
  CLKBUF cb(.A(clk), .Y(ck));
  // flop -> latch -> flop
  DFF r1(.CK(ck), .D(q4), .Q(q1));
  BUF u1(.A(q1), .Y(n1));
  BUF u2(.A(n1), .Y(n2));
  BUF u3(.A(n2), .Y(n3));
  LATCH l1(.G(ck), .D(n3), .Q(lq1));
  BUF u4(.A(lq1), .Y(m1));
  DFF r2(.CK(ck), .D(m1), .Q(q2));
  // latch -> negative latch -> output
  BUF u5(.A(lq1), .Y(m2));
  LATCHN l2(.GN(ck), .D(m2), .Q(lq2));
  BUF u6(.A(lq2), .Y(z));
  // input -> latch -> output
  BUF u7(.A(a), .Y(a1));
  LATCH l3(.G(ck), .D(a1), .Q(lq3));
  BUF u8(.A(lq3), .Y(y));
  // divide by 2 generated clock
  DFF div(.CK(ck), .D(divn), .Q(gclk));
  INV ui(.A(gclk), .Y(divn));
  CLKBUF gb(.A(gclk), .Y(gck));
  DFF r3(.CK(gck), .D(q2), .Q(q3));
  BUF u9(.A(q3), .Y(n9));
  DFF r4(.CK(gck), .D(n9), .Q(q4));
  DFF r5(.CK(gck), .D(b), .Q(q5));
  BUF u10(.A(q5), .Y(w));
  // flop -> latch reset -> flop
  BUF u12(.A(q2), .Y(rn1));
  BUF u13(.A(rn1), .Y(rn2));
  BUF u14(.A(rn2), .Y(rn3));
  LATCHR l4(.G(ck), .D(n2), .RN(rn3), .Q(lq4));
  BUF u11(.A(lq4), .Y(m4));
  DFF r6(.CK(ck), .D(m4), .Q(q6));
  // latch without a latch group
  LATCHI l5(.G(ck), .D(n1), .Q(lq5));
  DFF r7(.CK(ck), .D(lq5), .Q(q7));
endmodule
