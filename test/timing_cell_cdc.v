module timing_cell_cdc (input in, input clk_a, input clk_b, output out);

  wire w1;

  DFFHQx4_ASAP7_75t_R r1 (.D(in), .CLK(clk_a), .Q(w1));
  DFFHQx4_ASAP7_75t_R r2 (.D(w1), .CLK(clk_b), .Q(out));

endmodule
