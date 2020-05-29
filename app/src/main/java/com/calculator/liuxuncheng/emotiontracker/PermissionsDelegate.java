package com.calculator.liuxuncheng.emotiontracker;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.view.View;

class PermissionsDelegate {

    private static final int REQUEST_CODE_Camera = 10;
    private static final int REQUEST_CODE_Internet = 11;
    private final Activity activity;

    PermissionsDelegate(Activity activity) {
        this.activity = activity;
    }

    boolean hasCameraPermission() {
        int permissionCheckResult = ContextCompat.checkSelfPermission(
                activity,
                Manifest.permission.CAMERA
        );
        return permissionCheckResult == PackageManager.PERMISSION_GRANTED;
    }

    boolean hasInternetPermission() {
        int permissionCheckResult = ContextCompat.checkSelfPermission(
                activity,
                Manifest.permission.INTERNET
        );
        return permissionCheckResult == PackageManager.PERMISSION_GRANTED;
    }

    void requestCameraPermission() {
        ActivityCompat.requestPermissions(
                activity,
                new String[]{Manifest.permission.CAMERA},
                REQUEST_CODE_Camera
        );
    }

    void requestInternetPermission() {
        ActivityCompat.requestPermissions(
                activity,
                new String[]{Manifest.permission.INTERNET},
                REQUEST_CODE_Internet
        );
    }

    boolean resultGranted(int requestCode,
                          String[] permissions,
                          int[] grantResults) {

        switch (requestCode){
            case REQUEST_CODE_Camera:
                if (grantResults.length < 1) {
                    return false;
                }
                if (!(permissions[0].equals(Manifest.permission.CAMERA))) {
                    return false;
                }

                View noPermissionView = activity.findViewById(R.id.no_permission);

                if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    noPermissionView.setVisibility(View.GONE);
                    return true;
                }

                requestCameraPermission();
                noPermissionView.setVisibility(View.VISIBLE);
                return false;
            case REQUEST_CODE_Internet:
                if (grantResults.length < 1) {
                    return false;
                }
                if (!(permissions[0].equals(Manifest.permission.INTERNET))) {
                    return false;
                }
                if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    return true;
                }
                requestInternetPermission();
                return false;
        }
        return false;

    }
}

