/* test_cui_iso_api.c - the extension's own declarations must not borrow CUI's
 * import/export macro.
 *
 * On Windows with a shared CUI, CUI_API expands to __declspec(dllimport) for
 * everyone except CUI itself. That is right for CUI's functions and wrong for
 * the extension's own: MSVC refuses the definition of an imported symbol
 * (C2491), and a consumer looks for the function in CUI's DLL instead of in
 * the library it linked.
 *
 * So this file is compiled exactly as such a consumer would be -- with
 * CUI_SHARED defined -- and defines one of the extension's functions. If the
 * header still said CUI_API for it, MSVC stops the build right here. The
 * extension itself is not linked in: this translation unit is the definition
 * of the symbol, and linking the library would give it twice.
 *
 * On compilers that do not check the attribute the file still has to compile
 * and run; the configure-time check in CMakeLists.txt covers the same mistake
 * on every platform, by reading the headers.
 */
#include <cui/cui_iso.h>

void cui_iso_scene_reset(cui_iso_scene *s) {
    (void)s;
}

int main(void) { return 0; }
