import serial
import serial.tools.list_ports

def establishPort(port=None):
  if port:
    pdb_port = port
  else:
    available_ports = list(serial.tools.list_ports.comports())
    for port in available_ports:
      if "ItsyBitsy M0 Express" in port.description:
        pdb_port = port.device
  
  return serial.Serial(port=pdb_port, baudrate=115200, timeout=1)


def getBattInfo(serial_device):

  if serial_device.in_waiting:
  
    output = serial_device.readline().decode('ascii').strip()
    incoming = output[output.rfind("[") + 1 : len(output) - 1].split(",")
    data = {"Timestamp" : incoming[0], "Armed" : incoming[1], "Batt1V" : incoming[2], "Batt1Curr" : incoming[3], "Batt2V" : incoming[4], "Batt2Curr" : incoming[5], "RegTherm" : incoming[6], "MCTherm" : incoming[7]}
    return data
  else :
    return None


def main():

  serial_device = establishPort()

  while True: 
    x = getBattInfo(serial_device)
    if x:
      print(x)


if __name__ == "__main__":
  main()