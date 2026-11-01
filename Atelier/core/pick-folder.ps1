# Seletor de pasta do Windows (o moderno, do Explorer) pro Atelier. Escreve o caminho escolhido na
# saída; nada se ele cancelar. O dono é a janela da frente (a do Atelier), pra abrir por cima dela.
param([string]$Start = '', [string]$Title = 'Pasta do pedido')
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class AtelierFolderPicker {
  [ComImport, Guid("DC1C5A9C-E88A-4dde-A5A1-60F82A20AEF7")] class FileOpenDialogRCW {}
  [ComImport, Guid("42f85136-db7e-439c-85f1-e4075d135fc8"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IFileOpenDialog {
    [PreserveSig] int Show(IntPtr parent);
    void SetFileTypes(uint c, IntPtr f); void SetFileTypeIndex(uint i); void GetFileTypeIndex(out uint i);
    void Advise(IntPtr p, out uint c); void Unadvise(uint c);
    void SetOptions(uint fos); void GetOptions(out uint fos);
    void SetDefaultFolder(IShellItem si); void SetFolder(IShellItem si);
    void GetFolder(out IShellItem si); void GetCurrentSelection(out IShellItem si);
    void SetFileName([MarshalAs(UnmanagedType.LPWStr)] string n); void GetFileName([MarshalAs(UnmanagedType.LPWStr)] out string n);
    void SetTitle([MarshalAs(UnmanagedType.LPWStr)] string t); void SetOkButtonLabel([MarshalAs(UnmanagedType.LPWStr)] string t);
    void SetFileNameLabel([MarshalAs(UnmanagedType.LPWStr)] string t); void GetResult(out IShellItem si);
    void AddPlace(IShellItem si, int fdap); void SetDefaultExtension([MarshalAs(UnmanagedType.LPWStr)] string e);
    void Close(int hr); void SetClientGuid(ref Guid g); void ClearClientData(); void SetFilter(IntPtr f);
    void GetResults(out IntPtr e); void GetSelectedItems(out IntPtr e);
  }
  [ComImport, Guid("43826D1E-E718-42EE-BC55-A1E261C37BFE"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IShellItem {
    void BindToHandler(IntPtr pbc, ref Guid bhid, ref Guid riid, out IntPtr ppv);
    void GetParent(out IShellItem si);
    void GetDisplayName(uint sigdn, [MarshalAs(UnmanagedType.LPWStr)] out string name);
    void GetAttributes(uint mask, out uint attrs);
    void Compare(IShellItem si, uint hint, out int order);
  }
  [DllImport("shell32.dll", CharSet = CharSet.Unicode, PreserveSig = false)]
  static extern void SHCreateItemFromParsingName(string path, IntPtr pbc, ref Guid riid, out IShellItem item);
  [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
  public static string Pick(string start, string title, bool show) {
    var d = (IFileOpenDialog)new FileOpenDialogRCW();
    d.SetOptions(0x20 | 0x40 | 0x800); // pastas, só disco, tem de existir
    d.SetTitle(title);
    d.SetOkButtonLabel("Usar esta pasta");
    if (!string.IsNullOrEmpty(start) && System.IO.Directory.Exists(start)) {
      Guid g = typeof(IShellItem).GUID; IShellItem si;
      SHCreateItemFromParsingName(start, IntPtr.Zero, ref g, out si); d.SetFolder(si);
    }
    if (!show) return "";
    if (d.Show(GetForegroundWindow()) != 0) return null;
    IShellItem r; d.GetResult(out r);
    string p; r.GetDisplayName(0x80058000, out p);
    return p;
  }
}
"@
if ($Title -eq '--check') { [AtelierFolderPicker]::Pick($Start, 'teste', $false) | Out-Null; [Console]::Out.Write('ok'); exit 0 }
$p = [AtelierFolderPicker]::Pick($Start, $Title, $true)
if ($p) { [Console]::Out.Write($p) }
