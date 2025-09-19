module internal_paths_top_complex (input in, input clk, output out);

  wire w1, w2;

  internal_paths_cell t1 (.in(in), .clk(clk), .out(w1));

  BUFx2_ASAP7_75t_R b1 (.A(w1), .Y(w2));

  internal_paths_cell_another t2 (.in(w2), .clk(clk), .out(out));
  
endmodule