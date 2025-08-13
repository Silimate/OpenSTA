module timing_cell_complex (input in, input clk_a, input clk_b, output out);
  
  timing_cell_cdc tt1(.in(in), .clk_a(clk_a), .clk_b(clk_b), .out(out));

endmodule
