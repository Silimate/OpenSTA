module internal_paths_cell_another (input in, input clk, output out);

  wire w1, w2, w3, w4, w5, w6, w7, w8, w9;

  DFFHQx4_ASAP7_75t_R r1 (.D(in), .CLK(clk), .Q(w1));

  BUFx2_ASAP7_75t_R u2 (.A(w1), .Y(w2));
  BUFx2_ASAP7_75t_R u3 (.A(w2), .Y(w3));
  BUFx2_ASAP7_75t_R u5 (.A(w3), .Y(w4));
  BUFx2_ASAP7_75t_R u6 (.A(w4), .Y(w5));
  BUFx2_ASAP7_75t_R u7 (.A(w5), .Y(w6));
  BUFx2_ASAP7_75t_R u8 (.A(w6), .Y(w7));
  BUFx2_ASAP7_75t_R u9 (.A(w7), .Y(w8));
  
  // r1 - r2 with four delay buffers
  // path group: custom
  DFFHQx4_ASAP7_75t_R r2 (.D(w5), .CLK(clk), .Q(out));

  // r1 - r3 with seven delay buffers
  // path group: long
  DFFHQx4_ASAP7_75t_R r3 (.D(w8), .CLK(clk), .Q(w9));

  // register-register paths from this cell
  // won't be visible while exporting register-register
  // paths from the current one, if we will only
  // import it from the Liberty
  internal_paths_cell_hidden r4 (.in(w9), .clk(clk), .out(out));

endmodule
