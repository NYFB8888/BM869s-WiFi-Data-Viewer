# EspUsbHost Patch Instructions
## Why this patch is needed
`sendVendorOutput()` in EspUsbHost.cpp has a bug: it calls `sendHIDReport()`
internally which always uses EP0 (control transfer / SET_REPORT). The BU-86X
has an interrupt OUT endpoint (EP 0x01) and rejects EP0 writes with a STALL.
The fix makes `sendVendorOutput()` use the interrupt OUT endpoint directly.

## Step-by-step

1. Open this file in a text editor (Notepad++, VS Code, etc.):
   ```
   C:\Users\<you>\Documents\Arduino\libraries\EspUsbHost\src\EspUsbHost.cpp
   ```

2. Find the `sendVendorOutput` function (search for `bool EspUsbHost::sendVendorOutput`).
   It starts at approximately line 1800.

3. The function body currently contains something like:
   ```cpp
   uint8_t report[64] = {};
   report[0] = ESP_USB_HOST_HID_REPORT_ID_VENDOR;
   if (length > 0)
   {
     memcpy(report + 1, data, length);
   }
   return sendHIDReport(device->vendorInterfaceNumber,
                        ESP_USB_HOST_HID_REPORT_TYPE_OUTPUT,
                        0,
                        report,
                        length + 1,
                        device->info.address);
   ```

4. **Delete those lines** (everything between the opening `{` of the
   function body and the closing `}`) and replace with:
   ```cpp
   if (!device->hasVendorOutEndpoint)
   {
     ESP_LOGW(TAG, "sendVendorOutput() no interrupt OUT endpoint");
     return false;
   }

   usb_transfer_t *transfer = nullptr;
   esp_err_t err = usb_host_transfer_alloc(length, 0, &transfer);
   if (err != ESP_OK)
   {
     setLastError(err);
     return false;
   }

   memcpy(transfer->data_buffer, data, length);
   transfer->device_handle    = device->handle;
   transfer->bEndpointAddress = device->vendorOutEndpointAddress;
   transfer->callback         = controlTransferCallback;
   transfer->context          = this;
   transfer->num_bytes        = length;

   err = usb_host_transfer_submit(transfer);
   if (err != ESP_OK)
   {
     setLastError(err);
     usb_host_transfer_free(transfer);
     return false;
   }
   return true;
   ```

5. Make sure the closing `}` of the function is present after `return true;`.
   The next function (`bool EspUsbHost::sendVendorFeature`) should start
   immediately after.

6. Save the file and recompile — no other files need changing.

## Verification
After patching, compile and upload `BasicTest.ino`. You should see:
```
[TX] sendVendorOutput: OK
[HID] len=8  ...
*** LINK WORKS! ***
```
with no `E (xxxxx) USBH: Dev 1 EP 0 STALL` errors.
