// Force Thumb, seed functions from the Cortex-M vector table at the base of the image (word0 = SP, words 1.. = handlers),
// run auto-analysis and decompile every function to OUT_FILE. Read-only on the input binary.
// Usage: -postScript VecDecompileAll.java OUT_FILE [EXTRA_THUMB_ENTRY ...]
// @category GP100
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.mem.Memory;
import java.io.PrintWriter;
import java.math.BigInteger;

public class VecDecompileAll extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        Memory mem = currentProgram.getMemory();
        AddressSpace sp = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Register tmode = currentProgram.getLanguage().getRegister("TMode");
        Address min = mem.getMinAddress(), max = mem.getMaxAddress();
        currentProgram.getProgramContext().setValue(tmode, min, max, BigInteger.ONE);
        for (int i = 1; i < 64; i++) {
            long v = mem.getInt(min.add(i * 4L)) & 0xffffffffL;
            if ((v & 1) == 0 || v < min.getOffset() || v > max.getOffset()) continue;
            Address a = sp.getAddress(v & ~1L);
            disassemble(a);
            createFunction(a, i == 1 ? "Reset_Handler" : String.format("Vec_%02d", i));
        }
        for (int i = 1; i < args.length; i++) {
            Address a = toAddr(args[i]);
            disassemble(a);
            createFunction(a, String.format("X_%s", a));
        }
        analyzeAll(currentProgram);
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        try (PrintWriter out = new PrintWriter(args[0])) {
            FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
            int n = 0;
            while (it.hasNext()) {
                Function f = it.next();
                n++;
                out.printf("%n//==== %s @ %s (size %d) ====%n", f.getName(), f.getEntryPoint(), f.getBody().getNumAddresses());
                DecompileResults r = di.decompileFunction(f, 60, monitor);
                out.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// decompile failed: " + r.getErrorMessage());
            }
            out.printf("%n// total functions: %d%n", n);
        }
    }
}
