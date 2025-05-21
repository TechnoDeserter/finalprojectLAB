import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import 'package:wifi_scan/wifi_scan.dart';
import 'package:permission_handler/permission_handler.dart';

class Setup extends StatefulWidget {
  final String esp32Ip;
  final String ssid;
  final Function(String ssid, String password) resetWiFi;

  const Setup({
    super.key,
    required this.esp32Ip,
    required this.ssid,
    required this.resetWiFi,
  });

  @override
  _SetupState createState() => _SetupState();
}

class _SetupState extends State<Setup> {
  List<WiFiAccessPoint> _accessPoints = [];
  String? _selectedSsid;
  final TextEditingController _passwordController = TextEditingController();
  bool _obscurePassword = true;
  bool _isScanning = false;
  String _scanStatus = 'Tap "Refresh Networks" to scan';

  @override
  void initState() {
    super.initState();
    _selectedSsid = widget.ssid.isNotEmpty &&
            widget.ssid != "Error connecting" &&
            widget.ssid != "Unknown SSID"
        ? widget.ssid
        : null;
    _scanWiFiNetworks();
  }

  @override
  void dispose() {
    _passwordController.dispose();
    super.dispose();
  }

  Future<void> _scanWiFiNetworks() async {
    setState(() {
      _isScanning = true;
      _scanStatus = 'Scanning for Wi-Fi networks...';
    });

    // Request location permission
    var status = await Permission.locationWhenInUse.request();
    if (!status.isGranted) {
      setState(() {
        _isScanning = false;
        _scanStatus = 'Location permission denied';
        _accessPoints = [];
        _selectedSsid = null;
      });
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
            content:
                Text('Location permission required to scan Wi-Fi networks')),
      );
      return;
    }

    // Start Wi-Fi scan
    final canScan = await WiFiScan.instance.canGetScannedResults();
    if (canScan == CanGetScannedResults.yes) {
      final results = await WiFiScan.instance.getScannedResults();
      // Filter out duplicates and empty SSIDs
      final uniqueAccessPoints = <String, WiFiAccessPoint>{};
      for (var ap in results) {
        if (ap.ssid.isNotEmpty) {
          uniqueAccessPoints[ap.ssid] = ap;
        }
      }
      setState(() {
        _accessPoints = uniqueAccessPoints.values.toList();
        _isScanning = false;
        _scanStatus = _accessPoints.isEmpty
            ? 'No Wi-Fi networks found'
            : 'Networks found';
        // Ensure _selectedSsid is valid
        if (_selectedSsid != null &&
            !_accessPoints.any((ap) => ap.ssid == _selectedSsid)) {
          _selectedSsid =
              _accessPoints.isNotEmpty ? _accessPoints.first.ssid : null;
        }
      });
    } else {
      setState(() {
        _isScanning = false;
        _scanStatus = 'Cannot scan Wi-Fi networks';
        _accessPoints = [];
        _selectedSsid = null;
      });
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Wi-Fi scanning not available')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return SingleChildScrollView(
      padding: const EdgeInsets.fromLTRB(16, 13, 16, 80),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.center,
        children: [
          Text(
            'WiFi Settings',
            style: GoogleFonts.roboto(
              fontSize: 28,
              fontWeight: FontWeight.bold,
              color: Colors.purple[800],
              letterSpacing: 0.5,
            ),
          ),
          const SizedBox(height: 24),
          Card(
            elevation: 4,
            shadowColor: Colors.purple.withOpacity(0.1),
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.circular(12),
            ),
            margin: const EdgeInsets.symmetric(vertical: 8),
            child: Padding(
              padding: const EdgeInsets.all(16.0),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                    children: [
                      Text(
                        'Current IP',
                        style: GoogleFonts.roboto(
                          fontSize: 16,
                          fontWeight: FontWeight.w500,
                          color: Colors.purple,
                        ),
                      ),
                      Text(
                        widget.esp32Ip,
                        style: GoogleFonts.roboto(
                          fontSize: 16,
                          fontWeight: FontWeight.w600,
                          color: Colors.black87,
                        ),
                        overflow: TextOverflow.ellipsis,
                      ),
                    ],
                  ),
                  const SizedBox(height: 12),
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                    children: [
                      Text(
                        'SSID',
                        style: GoogleFonts.roboto(
                          fontSize: 16,
                          fontWeight: FontWeight.w500,
                          color: Colors.purple,
                        ),
                      ),
                      Expanded(
                        child: Padding(
                          padding: const EdgeInsets.only(left: 8.0),
                          child: _accessPoints.isEmpty
                              ? Text(
                                  _scanStatus,
                                  style: GoogleFonts.roboto(
                                    fontSize: 16,
                                    fontWeight: FontWeight.w600,
                                    color: Colors.black54,
                                  ),
                                  overflow: TextOverflow.ellipsis,
                                  textAlign: TextAlign.end,
                                )
                              : DropdownButton<String>(
                                  isExpanded: true,
                                  value: _selectedSsid,
                                  hint: Text(
                                    'Select Wi-Fi Network',
                                    style: GoogleFonts.roboto(
                                      fontSize: 16,
                                      color: Colors.black54,
                                    ),
                                  ),
                                  items: _accessPoints
                                      .map((ap) => DropdownMenuItem(
                                            value: ap.ssid,
                                            child: Text(
                                              ap.ssid,
                                              style: GoogleFonts.roboto(
                                                fontSize: 16,
                                                fontWeight: FontWeight.w600,
                                                color: Colors.black87,
                                              ),
                                              overflow: TextOverflow.ellipsis,
                                            ),
                                          ))
                                      .toList(),
                                  onChanged: (value) {
                                    setState(() {
                                      _selectedSsid = value;
                                    });
                                  },
                                ),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 12),
                  Text(
                    'Password',
                    style: GoogleFonts.roboto(
                      fontSize: 16,
                      fontWeight: FontWeight.w500,
                      color: Colors.purple,
                    ),
                  ),
                  const SizedBox(height: 8),
                  TextField(
                    controller: _passwordController,
                    obscureText: _obscurePassword,
                    decoration: InputDecoration(
                      border: OutlineInputBorder(
                        borderRadius: BorderRadius.circular(8),
                        borderSide: BorderSide(color: Colors.purple[600]!),
                      ),
                      hintText: 'Enter Wi-Fi Password',
                      hintStyle: GoogleFonts.roboto(
                        color: Colors.black54,
                      ),
                      suffixIcon: IconButton(
                        icon: Icon(
                          _obscurePassword
                              ? Icons.visibility
                              : Icons.visibility_off,
                          color: Colors.purple[600],
                        ),
                        onPressed: () {
                          setState(() {
                            _obscurePassword = !_obscurePassword;
                          });
                        },
                      ),
                    ),
                    style: GoogleFonts.roboto(
                      fontSize: 16,
                      color: Colors.black87,
                    ),
                  ),
                  const SizedBox(height: 12),
                  Text(
                    _scanStatus,
                    style: GoogleFonts.roboto(
                      fontSize: 14,
                      color: _isScanning ? Colors.purple[600] : Colors.black54,
                    ),
                  ),
                  const SizedBox(height: 16),
                  Center(
                    child: ElevatedButton(
                      onPressed: _isScanning
                          ? null
                          : () {
                              if (_selectedSsid == null ||
                                  _selectedSsid!.isEmpty) {
                                ScaffoldMessenger.of(context).showSnackBar(
                                  const SnackBar(
                                      content: Text(
                                          'Please select a Wi-Fi network')),
                                );
                                return;
                              }
                              if (_passwordController.text.isEmpty) {
                                ScaffoldMessenger.of(context).showSnackBar(
                                  const SnackBar(
                                      content: Text('Please enter a password')),
                                );
                                return;
                              }
                              widget.resetWiFi(
                                  _selectedSsid!, _passwordController.text);
                              ScaffoldMessenger.of(context).showSnackBar(
                                SnackBar(
                                  content: Text(
                                      'Wi-Fi settings updated: SSID=$_selectedSsid'),
                                ),
                              );
                            },
                      style: ElevatedButton.styleFrom(
                        padding: const EdgeInsets.symmetric(
                            horizontal: 24, vertical: 12),
                        backgroundColor: Colors.purple,
                        foregroundColor: Colors.white,
                        shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(8),
                        ),
                        elevation: 2,
                        shadowColor: Colors.purple.withOpacity(0.3),
                      ),
                      child: Row(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          const Icon(Icons.wifi, size: 18),
                          const SizedBox(width: 8),
                          Text(
                            'Save WiFi Settings',
                            style: GoogleFonts.roboto(
                              fontSize: 16,
                              fontWeight: FontWeight.w600,
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                  const SizedBox(height: 12),
                  Center(
                    child: TextButton(
                      onPressed: _isScanning ? null : _scanWiFiNetworks,
                      child: Text(
                        'Refresh Networks',
                        style: GoogleFonts.roboto(
                          fontSize: 16,
                          color: Colors.purple[600],
                          fontWeight: FontWeight.w600,
                        ),
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}
