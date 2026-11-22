// Decompile functions at the given addresses (script args after the output path), with disassembly of
// the first N instructions when the decompile is unhelpful. Read-only.
// Usage: -postScript DecompileAt.java OUT_FILE ADDR [ADDR...]
// @category GP100
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import java.io.PrintWriter;

public class DecompileAt extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        try (PrintWriter out = new PrintWriter(args[0])) {
            DecompInterface di = new DecompInterface();
            di.openProgram(currentProgram);
            for (int i = 1; i < args.length; i++) {
                Address a = toAddr(args[i].startsWith("+") ? args[i].substring(1) : args[i]);
                Function f = getFunctionContaining(a);
                if (f == null && args[i].startsWith("+")) {
                    // "+ADDR" asks to create a function at a Thumb entry (jump-table targets are not discovered).
                    a = toAddr(args[i].substring(1));
                    ghidra.program.model.lang.Register tm = currentProgram.getLanguage().getRegister("TMode");
                    if (tm != null) {
                        try {
                            currentProgram.getProgramContext().setValue(tm, currentProgram.getMinAddress(),
                                currentProgram.getMaxAddress(), java.math.BigInteger.ONE);
                        } catch (Exception ignored) {
                            // already set (projects where instructions exist): nothing to do
                        }
                    }
                    disassemble(a);
                    f = createFunction(a, String.format("T_%s", a));
                    if (f == null) f = getFunctionContaining(a);
                }
                if (f == null) {
                    out.printf("%n==== %s: no function, raw disassembly ====%n", a);
                    Instruction ins = getInstructionAt(a);
                    for (int n = 0; n < 40 && ins != null; n++, ins = ins.getNext())
                        out.printf("%s  %s%n", ins.getAddress(), ins);
                    continue;
                }
                out.printf("%n==== %s @ %s ====%n", f.getName(), f.getEntryPoint());
                DecompileResults r = di.decompileFunction(f, 60, monitor);
                out.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "decompile failed: " + r.getErrorMessage());
            }
        }
    }
}
