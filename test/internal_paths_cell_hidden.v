module internal_paths_cell_hidden (input in, input clk, output out);

  wire w1, w2, w3, w4, w5, w6, w7, w8;

  DFFHQx4_ASAP7_75t_R r1 (.D(in), .CLK(clk), .Q(w1));

  BUFx2_ASAP7_75t_R u2 (.A(w1), .Y(w2));
  BUFx2_ASAP7_75t_R u3 (.A(w2), .Y(w3));
  BUFx2_ASAP7_75t_R u5 (.A(w3), .Y(w4));
  BUFx2_ASAP7_75t_R u6 (.A(w4), .Y(w5));
  BUFx2_ASAP7_75t_R u7 (.A(w5), .Y(w6));
  BUFx2_ASAP7_75t_R u8 (.A(w6), .Y(w7));
  BUFx2_ASAP7_75t_R u9 (.A(w7), .Y(w8));
  BUFx2_ASAP7_75t_R u10 (.A(w8), .Y(w9));
  
  // r1 - r2 with eight delay buffers
  // path group: custom
  DFFHQx4_ASAP7_75t_R r2 (.D(w9), .CLK(clk), .Q(out));

endmodule
