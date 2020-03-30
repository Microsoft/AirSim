import setup_path 
import airsim


class PositionSet:
      def __init__(self, initX, initY, initZ,port):
          self.initX = initX
          self.initY = initY
          self.initZ = initZ
          self.port = port

      def start(self):
          print("position_set")
          client = airsim.MultirotorClient('',self.port)
          client.confirmConnection()
          client.enableApiControl(True)
          client.armDisarm(True) 
          print( -1 * float(self.initZ))
          client.moveToPositionAsync(float(self.initX),float(self.initY), -1 * float(self.initZ), 5).join()

          print("simSetCameraOrientation")
          position = client.getMultirotorState().kinematics_estimated.position;
          print(position)  

          return "complit"