import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';
import 'package:animate_do/animate_do.dart';
import 'package:wifi_scan/wifi_scan.dart';


void main() {
  runApp(const RelayControlApp());
}

class RelayControlApp extends StatelessWidget {
  const RelayControlApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Relay Control',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.teal),
        useMaterial3: true,
        cardTheme: CardTheme(
          elevation: 2,
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
        ),
        elevatedButtonTheme: ElevatedButtonThemeData(
          style: ElevatedButton.styleFrom(
            padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 15),
            shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
          ),
        ),
      ),
      debugShowCheckedModeBanner: false,
      home: const HomePage(),
    );
  }
}

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  bool _isSetupComplete = false;

  @override
  void initState() {
    super.initState();
    _checkSetupStatus();
  }

  Future<void> _checkSetupStatus() async {
    final prefs = await SharedPreferences.getInstance();
    final savedIp = prefs.getString('esp32Ip');
    setState(() {
      _isSetupComplete = savedIp != null && savedIp.isNotEmpty;
    });
  }

  @override
  Widget build(BuildContext context) {
    return _isSetupComplete ? const RelayControlPage() : const SetupScreen();
  }
}

class SetupScreen extends StatefulWidget {
  const SetupScreen({super.key});

  @override
  State<SetupScreen> createState() => _SetupScreenState();
}

class _SetupScreenState extends State<SetupScreen> {
  final TextEditingController _ipController = TextEditingController(text: '192.168.4.1');
  final TextEditingController _ssidController = TextEditingController();
  final TextEditingController _passwordController = TextEditingController();
  bool _isLoading = false;
  List<String> _wifiNetworks = [];
  bool _isPasswordVisible = false;

  @override
  void initState() {
    super.initState();
    _loadWiFiNetworks();
  }

  Future<void> _loadWiFiNetworks() async {
    try {
      // Check if scanning is possible and request permissions if needed
      final canScan = await WiFiScan.instance.canStartScan(askPermissions: true);
      if (canScan != CanStartScan.yes) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Cannot start WiFi scan. Check permissions.')),
        );
        return;
      }

      // Start WiFi scan
      final isScanning = await WiFiScan.instance.startScan();
      if (!isScanning) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Failed to start WiFi scan.')),
        );
        return;
      }

      // Get scanned results
      final canGetResults = await WiFiScan.instance.canGetScannedResults(askPermissions: true);
      if (canGetResults == CanGetScannedResults.yes) {
        final accessPoints = await WiFiScan.instance.getScannedResults();
        setState(() {
          _wifiNetworks = accessPoints.map((ap) => ap.ssid).where((ssid) => ssid.isNotEmpty).toList();
        });
      } else {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Cannot retrieve WiFi scan results. Check permissions.')),
        );
      }
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Error loading WiFi networks: $e')),
      );
    }
  }

  Future<void> _saveCredentials() async {
    final ip = _ipController.text;
    final ssid = _ssidController.text;
    final password = _passwordController.text;

    if (!RegExp(r'^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$').hasMatch(ip)) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Invalid IP address format')),
      );
      return;
    }

    if (ssid.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('SSID cannot be empty')),
      );
      return;
    }

    setState(() => _isLoading = true);
    try {
      // Send WiFi credentials to ESP32 directly (assuming phone is connected to ESP32 AP)
      final response = await http.post(
        Uri.parse('http://$ip/setWiFi'),
        body: jsonEncode({
          'ssid': ssid,
          'password': password,
        }),
        headers: {'Content-Type': 'application/json'},
      ).timeout(const Duration(seconds: 10));

      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        if (data['success']) {
          final newIp = data['ip'];
          final prefs = await SharedPreferences.getInstance();
          await prefs.setString('esp32Ip', newIp);
          Navigator.pushReplacement(
            context,
            MaterialPageRoute(builder: (context) => const RelayControlPage()),
          );
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('WiFi credentials saved. Connected to ESP32.')),
          );
        } else {
          throw Exception('Failed to set WiFi: ${data['error']}');
        }
      } else {
        throw Exception('Failed to set WiFi: ${response.statusCode}');
      }
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Error connecting to ESP32: $e')),
      );
    } finally {
      setState(() => _isLoading = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('ESP32 WiFi Setup'),
        centerTitle: true,
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: SingleChildScrollView(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Text(
                'Configure ESP32 WiFi',
                style: Theme.of(context).textTheme.headlineSmall?.copyWith(fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 20),
              TextField(
                controller: _ipController,
                decoration: const InputDecoration(
                  labelText: 'ESP32 AP IP Address',
                  hintText: 'e.g., 192.168.4.1',
                  border: OutlineInputBorder(),
                ),
                keyboardType: TextInputType.numberWithOptions(decimal: true),
              ),
              const SizedBox(height: 20),
              DropdownButtonFormField<String>(
                decoration: const InputDecoration(
                  labelText: 'WiFi SSID',
                  border: OutlineInputBorder(),
                ),
                items: _wifiNetworks.map((ssid) {
                  return DropdownMenuItem<String>(
                    value: ssid,
                    child: Text(ssid.isEmpty ? 'Unknown' : ssid),
                  );
                }).toList()
                  ..add(const DropdownMenuItem<String>(
                    value: 'manual',
                    child: Text('Enter manually'),
                  )),
                onChanged: (value) {
                  if (value == 'manual') {
                    _ssidController.clear();
                  } else {
                    _ssidController.text = value ?? '';
                  }
                },
                hint: const Text('Select WiFi Network'),
              ),
              const SizedBox(height: 20),
              TextField(
                controller: _ssidController,
                decoration: const InputDecoration(
                  labelText: 'WiFi SSID (if manual)',
                  border: OutlineInputBorder(),
                ),
              ),
              const SizedBox(height: 20),
              TextField(
                controller: _passwordController,
                decoration: InputDecoration(
                  labelText: 'WiFi Password',
                  border: const OutlineInputBorder(),
                  suffixIcon: IconButton(
                    icon: Icon(_isPasswordVisible ? Icons.visibility : Icons.visibility_off),
                    onPressed: () {
                      setState(() {
                        _isPasswordVisible = !_isPasswordVisible;
                      });
                    },
                  ),
                ),
                obscureText: !_isPasswordVisible,
              ),
              const SizedBox(height: 20),
              _isLoading
                  ? const CircularProgressIndicator()
                  : ElevatedButton(
                onPressed: _saveCredentials,
                child: const Text('Save and Connect'),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class RelayControlPage extends StatefulWidget {
  const RelayControlPage({super.key});

  @override
  State<RelayControlPage> createState() => _RelayControlPageState();
}

class _RelayControlPageState extends State<RelayControlPage> {
  String esp32Ip = "192.168.4.1";
  String ssid = "Loading...";
  bool isLoading = false;
  List<bool> relayStates = [false, false, false, false];
  List<TimeOfDay> onTimes = [
    const TimeOfDay(hour: 8, minute: 0),
    const TimeOfDay(hour: 9, minute: 0),
    const TimeOfDay(hour: 10, minute: 0),
    const TimeOfDay(hour: 11, minute: 0),
  ];
  List<TimeOfDay> offTimes = [
    const TimeOfDay(hour: 18, minute: 0),
    const TimeOfDay(hour: 19, minute: 0),
    const TimeOfDay(hour: 20, minute: 0),
    const TimeOfDay(hour: 21, minute: 0),
  ];

  @override
  void initState() {
    super.initState();
    _loadIp();
  }

  Future<void> _loadIp() async {
    final prefs = await SharedPreferences.getInstance();
    setState(() {
      esp32Ip = prefs.getString('esp32Ip') ?? esp32Ip;
    });
    _fetchInitialData();
  }

  Future<void> _saveIp(String ip) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('esp32Ip', ip);
    setState(() {
      esp32Ip = ip;
    });
    await _fetchInitialData();
  }

  Future<void> _fetchInitialData() async {
    setState(() => isLoading = true);
    try {
      final response = await http.get(Uri.parse('http://$esp32Ip/')).timeout(const Duration(seconds: 5));
      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        setState(() {
          ssid = data['ssid'] ?? 'Unknown';
          isLoading = false;
        });
      } else {
        throw Exception('Failed to fetch data: ${response.statusCode}');
      }
    } catch (e) {
      setState(() {
        ssid = "Error connecting";
        isLoading = false;
      });
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Error connecting to ESP32: $e'),
          action: SnackBarAction(label: 'Retry', onPressed: _fetchInitialData),
        ),
      );
    }
  }

  Future<void> _selectTime(BuildContext context, int relayIndex, bool isOnTime) async {
    final TimeOfDay? picked = await showTimePicker(
      context: context,
      initialTime: isOnTime ? onTimes[relayIndex] : offTimes[relayIndex],
      builder: (context, child) {
        return Theme(
          data: Theme.of(context).copyWith(
            colorScheme: ColorScheme.light(
              primary: Colors.teal,
              onPrimary: Colors.white,
              surface: Colors.teal.shade50,
            ),
          ),
          child: child!,
        );
      },
    );
    if (picked != null) {
      setState(() {
        if (isOnTime) {
          onTimes[relayIndex] = picked;
        } else {
          offTimes[relayIndex] = picked;
        }
      });
    }
  }

  Future<void> _sendSettingsToESP() async {
    setState(() => isLoading = true);
    try {
      final Map<String, String> data = {
        'onTime1': _formatTime(onTimes[0]),
        'offTime1': _formatTime(offTimes[0]),
        'onTime2': _formatTime(onTimes[1]),
        'offTime2': _formatTime(offTimes[1]),
        'onTime3': _formatTime(onTimes[2]),
        'offTime3': _formatTime(offTimes[2]),
        'onTime4': _formatTime(onTimes[3]),
        'offTime4': _formatTime(offTimes[3]),
      };

      final response = await http.post(
        Uri.parse('http://$esp32Ip/set'),
        body: jsonEncode(data),
        headers: {'Content-Type': 'application/json'},
      ).timeout(const Duration(seconds: 5));

      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        if (data['success']) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('Settings saved successfully!')),
          );
        } else {
          throw Exception('Failed to save settings: ${data['error']}');
        }
      } else {
        throw Exception('Failed to save settings: ${response.statusCode}');
      }
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Error: $e'),
          action: SnackBarAction(label: 'Retry', onPressed: _sendSettingsToESP),
        ),
      );
    } finally {
      setState(() => isLoading = false);
    }
  }

  Future<void> _toggleRelay(int index, bool value) async {
    setState(() => isLoading = true);
    try {
      final response = await http.post(
        Uri.parse('http://$esp32Ip/toggle'),
        body: jsonEncode({
          'relay': (index + 1).toString(),
          'state': value ? '1' : '0',
        }),
        headers: {'Content-Type': 'application/json'},
      ).timeout(const Duration(seconds: 5));

      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        if (data['success']) {
          setState(() {
            relayStates[index] = value;
          });
        } else {
          throw Exception('Failed to toggle relay: ${data['error']}');
        }
      } else {
        throw Exception('Failed to toggle relay: ${response.statusCode}');
      }
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Error toggling relay: $e'),
          action: SnackBarAction(label: 'Retry', onPressed: () => _toggleRelay(index, value)),
        ),
      );
    } finally {
      setState(() => isLoading = false);
    }
  }

  Future<void> _resetWiFi() async {
    setState(() => isLoading = true);
    try {
      final response = await http.get(Uri.parse('http://$esp32Ip/reset')).timeout(const Duration(seconds: 5));
      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        if (data['success']) {
          final prefs = await SharedPreferences.getInstance();
          await prefs.remove('esp32Ip');
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('WiFi reset. Returning to setup screen.')),
          );
          Navigator.pushReplacement(
            context,
            MaterialPageRoute(builder: (context) => const SetupScreen()),
          );
        } else {
          throw Exception('Failed to reset WiFi: ${data['error']}');
        }
      } else {
        throw Exception('Failed to reset WiFi: ${response.statusCode}');
      }
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Error resetting WiFi: $e'),
          action: SnackBarAction(label: 'Retry', onPressed: _resetWiFi),
        ),
      );
    } finally {
      setState(() => isLoading = false);
    }
  }

  String _formatTime(TimeOfDay time) {
    return '${time.hour.toString().padLeft(2, '0')}:${time.minute.toString().padLeft(2, '0')}';
  }

  Future<void> _updateIpDialog() async {
    final controller = TextEditingController(text: esp32Ip);
    await showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Update ESP32 IP'),
        content: TextField(
          controller: controller,
          decoration: const InputDecoration(hintText: 'Enter ESP32 IP'),
          keyboardType: TextInputType.numberWithOptions(decimal: true),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
          ElevatedButton(
            onPressed: () {
              if (RegExp(r'^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$').hasMatch(controller.text)) {
                _saveIp(controller.text);
                Navigator.pop(context);
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(content: Text('IP updated. Connecting to ESP32...')),
                );
              } else {
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(content: Text('Invalid IP address format')),
                );
              }
            },
            child: const Text('Save'),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Relay Control'),
        centerTitle: true,
        actions: [
          IconButton(
            icon: const Icon(Icons.settings),
            onPressed: _updateIpDialog,
            tooltip: 'Change ESP32 IP',
          ),
        ],
      ),
      body: isLoading
          ? const Center(child: CircularProgressIndicator())
          : FadeIn(
        child: SingleChildScrollView(
          padding: const EdgeInsets.all(16.0),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                'Control Relays',
                style: Theme.of(context).textTheme.headlineSmall?.copyWith(fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 16),
              ...List.generate(4, (index) => _buildRelayControl(index)),
              const SizedBox(height: 24),
              Center(
                child: ElevatedButton.icon(
                  icon: const Icon(Icons.save),
                  label: const Text('Save All Settings'),
                  onPressed: _sendSettingsToESP,
                ),
              ),
              const SizedBox(height: 24),
              Card(
                child: Padding(
                  padding: const EdgeInsets.all(16.0),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        'Network Info',
                        style: Theme.of(context).textTheme.titleMedium?.copyWith(fontWeight: FontWeight.bold),
                      ),
                      const SizedBox(height: 8),
                      Text('IP: $esp32Ip'),
                      Text('SSID: $ssid'),
                      const SizedBox(height: 8),
                      TextButton.icon(
                        icon: const Icon(Icons.wifi_off),
                        label: const Text('Reset WiFi'),
                        onPressed: _resetWiFi,
                      ),
                    ],
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildRelayControl(int index) {
    return Card(
      margin: const EdgeInsets.only(bottom: 16),
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Relay ${index + 1}',
              style: Theme.of(context).textTheme.titleLarge?.copyWith(fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 12),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                const Text('ON Time:'),
                OutlinedButton(
                  onPressed: () => _selectTime(context, index, true),
                  child: Text(_formatTime(onTimes[index])),
                ),
              ],
            ),
            const SizedBox(height: 8),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                const Text('OFF Time:'),
                OutlinedButton(
                  onPressed: () => _selectTime(context, index, false),
                  child: Text(_formatTime(offTimes[index])),
                ),
              ],
            ),
            const SizedBox(height: 12),
            SwitchListTile(
              title: Text('Relay ${index + 1} State'),
              value: relayStates[index],
              onChanged: (value) => _toggleRelay(index, value),
              activeColor: Theme.of(context).colorScheme.primary,
            ),
          ],
        ),
      ),
    );
  }
}