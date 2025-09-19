module internal_paths_cell (input in, input clk, output out);

  wire w1, w2, w3, w4, w5, w6, w7;

  DFFHQx4_ASAP7_75t_R r1 (.D(in), .CLK(clk), .Q(w1));

  BUFx2_ASAP7_75t_R u2 (.A(w1), .Y(w2));
  BUFx2_ASAP7_75t_R u3 (.A(w2), .Y(w3));
  BUFx2_ASAP7_75t_R u5 (.A(w3), .Y(w4));
  BUFx2_ASAP7_75t_R u6 (.A(w4), .Y(w5));
  BUFx2_ASAP7_75t_R u7 (.A(w5), .Y(w6));
  BUFx2_ASAP7_75t_R u8 (.A(w6), .Y(w7));
  
  // r1 - r2 with one delay buffer
  // path group: clk
  DFFHQx4_ASAP7_75t_R r2 (.D(w2), .CLK(clk), .Q(out));

  // r1 - r3 with two delay buffers
  // path group: custom
  DFFHQx4_ASAP7_75t_R r3 (.D(w3), .CLK(clk), .Q(out));

  // r1 - r4 with three delay buffers
  // path group: custom
  DFFHQx4_ASAP7_75t_R r4 (.D(w4), .CLK(clk), .Q(out));

  // r1 - r5 with four delay buffers
  // path group: long
  DFFHQx4_ASAP7_75t_R r5 (.D(w5), .CLK(clk), .Q(out));

  // r1 - r6 with six delay buffers
  // path group: long
  DFFHQx4_ASAP7_75t_R r6 (.D(w7), .CLK(clk), .Q(out));

endmodule
