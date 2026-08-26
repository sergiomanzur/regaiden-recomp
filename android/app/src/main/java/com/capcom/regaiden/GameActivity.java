package com.capcom.regaiden;

import android.Manifest;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.security.MessageDigest;

public class GameActivity extends SDLActivity {

    private static final String TAG = "ReGaiden";
    public static final String ROM_FILENAME = "Resident Evil Gaiden (USA).gbc";
    public static final int RE_GAIDEN_ROM_SIZE = 2097152;
    public static final String RE_GAIDEN_SHA256 = "9a97678cbd8da02c8763e977674e17f460c06ea8b73bad35c52fe6817f506d44";

    private static final int REQUEST_CODE_PERMISSIONS = 1001;
    private static final int REQUEST_CODE_PICK_ROM = 1002;

    private static GameActivity sInstance = null;
    private boolean mRomReady = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        sInstance = this;
        super.onCreate(savedInstanceState);
        setupEdgeToEdge();

        if (checkAndSetupRom()) {
            mRomReady = true;
        } else {
            mRomReady = false;
            requestPermissionsAndPromptRom();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        setupEdgeToEdge();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            setupEdgeToEdge();
        }
    }

    private void setupEdgeToEdge() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }

        View decorView = getWindow().getDecorView();
        if (decorView != null) {
            int uiOptions = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                          | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                          | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                          | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                          | View.SYSTEM_UI_FLAG_FULLSCREEN
                          | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
            decorView.setSystemUiVisibility(uiOptions);
        }
    }

    private boolean checkAndSetupRom() {
        // 1. Check internal storage
        File internalRom = new File(getFilesDir(), ROM_FILENAME);
        if (isValidRomFile(internalRom)) {
            Log.i(TAG, "Valid ROM found in internal storage: " + internalRom.getAbsolutePath());
            return true;
        }

        // 2. Check candidate external storage paths
        String[] candidatePaths = {
            "/sdcard/ROMs/GBC/" + ROM_FILENAME,
            "/sdcard/Roms/GBC/" + ROM_FILENAME,
            "/sdcard/ROMs/GBC/rom.gbc",
            "/sdcard/Download/" + ROM_FILENAME,
            "/sdcard/Download/rom.gbc",
            "/sdcard/" + ROM_FILENAME,
            new File(getExternalFilesDir(null), ROM_FILENAME).getAbsolutePath()
        };

        for (String path : candidatePaths) {
            File file = new File(path);
            if (isValidRomFile(file)) {
                Log.i(TAG, "Valid ROM found at: " + path + ". Caching to internal storage...");
                if (copyFileToInternal(file, internalRom)) {
                    return true;
                }
            }
        }

        return false;
    }

    private boolean isValidRomFile(File file) {
        if (file == null || !file.exists() || !file.isFile()) {
            return false;
        }
        if (file.length() != RE_GAIDEN_ROM_SIZE) {
            return false;
        }
        try (InputStream in = new FileInputStream(file)) {
            byte[] buffer = new byte[RE_GAIDEN_ROM_SIZE];
            int total = 0;
            while (total < RE_GAIDEN_ROM_SIZE) {
                int read = in.read(buffer, total, RE_GAIDEN_ROM_SIZE - total);
                if (read < 0) break;
                total += read;
            }
            if (total == RE_GAIDEN_ROM_SIZE) {
                return validateRomBytes(buffer);
            }
        } catch (Exception e) {
            Log.e(TAG, "Error checking ROM file " + file.getAbsolutePath(), e);
        }
        return false;
    }

    public static boolean validateRomBytes(byte[] data) {
        if (data == null || data.length != RE_GAIDEN_ROM_SIZE) {
            return false;
        }
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            byte[] hash = md.digest(data);
            StringBuilder sb = new StringBuilder();
            for (byte b : hash) {
                sb.append(String.format("%02x", b));
            }
            return sb.toString().equalsIgnoreCase(RE_GAIDEN_SHA256);
        } catch (Exception e) {
            Log.e(TAG, "SHA256 calculation failed", e);
            return false;
        }
    }

    private boolean copyFileToInternal(File source, File dest) {
        try (InputStream in = new FileInputStream(source);
             OutputStream out = new FileOutputStream(dest)) {
            byte[] buf = new byte[8192];
            int len;
            while ((len = in.read(buf)) > 0) {
                out.write(buf, 0, len);
            }
            out.flush();
            return true;
        } catch (Exception e) {
            Log.e(TAG, "Failed to copy ROM to internal storage", e);
            return false;
        }
    }

    private void requestPermissionsAndPromptRom() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && Build.VERSION.SDK_INT <= Build.VERSION_CODES.Q) {
            if (checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(new String[]{
                    Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE
                }, REQUEST_CODE_PERMISSIONS);
                return;
            }
        }
        showRomPromptDialog();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_CODE_PERMISSIONS) {
            if (checkAndSetupRom()) {
                startVerifiedGame();
            } else {
                showRomPromptDialog();
            }
        }
    }

    private void showRomPromptDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("Resident Evil Gaiden — ROM Setup");
        builder.setMessage(
            "This application is an open-source recompilation and contains NO copyrighted game data, ROMs, or assets.\n\n" +
            "To play, please locate your legally owned 'Resident Evil Gaiden (USA)' Game Boy Color ROM (.gbc).\n\n" +
            "Expected Size: 2,097,152 bytes\n" +
            "Expected SHA256: " + RE_GAIDEN_SHA256
        );
        builder.setCancelable(false);
        builder.setPositiveButton("Locate ROM (.gbc)", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                launchRomPicker();
            }
        });
        builder.setNegativeButton("Exit", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                finish();
            }
        });
        builder.show();
    }

    public void launchRomPicker() {
        try {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            String[] mimeTypes = {"application/octet-stream", "*/*"};
            intent.putExtra(Intent.EXTRA_MIME_TYPES, mimeTypes);
            startActivityForResult(intent, REQUEST_CODE_PICK_ROM);
        } catch (Exception e) {
            Log.e(TAG, "ACTION_OPEN_DOCUMENT failed, falling back to ACTION_GET_CONTENT", e);
            Intent fallback = new Intent(Intent.ACTION_GET_CONTENT);
            fallback.setType("*/*");
            startActivityForResult(fallback, REQUEST_CODE_PICK_ROM);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode == REQUEST_CODE_PICK_ROM) {
            if (resultCode == RESULT_OK && data != null && data.getData() != null) {
                handlePickedRomUri(data.getData());
            } else {
                if (!mRomReady) {
                    showRomPromptDialog();
                }
            }
        }
    }

    private void handlePickedRomUri(Uri uri) {
        try (InputStream in = getContentResolver().openInputStream(uri)) {
            if (in == null) {
                showErrorDialog("Could not read the selected file stream.");
                return;
            }

            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            byte[] buf = new byte[8192];
            int len;
            while ((len = in.read(buf)) > 0) {
                baos.write(buf, 0, len);
                if (baos.size() > RE_GAIDEN_ROM_SIZE + 1024) {
                    break;
                }
            }
            byte[] romBytes = baos.toByteArray();

            if (validateRomBytes(romBytes)) {
                File internalRom = new File(getFilesDir(), ROM_FILENAME);
                try (FileOutputStream fos = new FileOutputStream(internalRom)) {
                    fos.write(romBytes);
                    fos.flush();
                }
                Log.i(TAG, "Successfully verified and saved ROM: " + internalRom.getAbsolutePath());
                Toast.makeText(this, "ROM verified successfully! Starting Resident Evil Gaiden...", Toast.LENGTH_SHORT).show();
                startVerifiedGame();
            } else {
                showErrorDialog(
                    "The selected file does not match the expected 'Resident Evil Gaiden (USA)' ROM.\n\n" +
                    "Expected Size: 2,097,152 bytes\n" +
                    "Actual Size: " + romBytes.length + " bytes\n" +
                    "Expected SHA256: " + RE_GAIDEN_SHA256
                );
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to read picked ROM URI", e);
            showErrorDialog("Failed to load file: " + e.getMessage());
        }
    }

    private void showErrorDialog(String message) {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("Invalid ROM File");
        builder.setMessage(message);
        builder.setCancelable(false);
        builder.setPositiveButton("Select Another File", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                launchRomPicker();
            }
        });
        builder.setNegativeButton("Exit", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                finish();
            }
        });
        builder.show();
    }

    private void startVerifiedGame() {
        Intent restartIntent = getIntent();
        finish();
        startActivity(restartIntent);
    }

    // JNI Native Call: called from ImGui settings menu if user taps "Select / Change ROM Image..."
    public static void requestRomPickerFromNative() {
        if (sInstance != null) {
            sInstance.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    sInstance.launchRomPicker();
                }
            });
        }
    }

    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL2",
            "main"
        };
    }

    @Override
    protected String getMainSharedObject() {
        return "libmain.so";
    }
}
