module timing_cell (input in, input clk, output out);

  wire clk_d, w1, w2, w3;

  BUFx2_ASAP7_75t_R b1 (.A(clk), .Y(clk_d));

  DFFHQx4_ASAP7_75t_R d1 (.D(in), .Q(w1), .CLK(clk_d));

  BUFx2_ASAP7_75t_R b2 (.A(w1), .Y(w2));
  BUFx2_ASAP7_75t_R b3 (.A(w2), .Y(w3));

  DFFHQx4_ASAP7_75t_R d2 (.D(w3), .Q(out), .CLK(clk_d));

endmodule
