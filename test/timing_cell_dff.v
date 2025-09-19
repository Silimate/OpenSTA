module timing_cell_dff (in, clk, out);
  input in, clk;
  output out;
  wire w1, w2;

  BUFx2_ASAP7_75t_R u1 (.A(in), .Y(w1));
  DFFHQx4_ASAP7_75t_R r1 (.D(w1), .CLK(clk), .Q(w2));
  BUFx2_ASAP7_75t_R u2 (.A(w2), .Y(out));
endmodule
