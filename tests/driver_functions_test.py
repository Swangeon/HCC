import unittest
import select

class TestDriver(unittest.TestCase):
    def setUp(self):
        self.driver = open("/dev/tap0")

    def test_read_from_driver(self):
        # Mock some data in the driver
        self.driver.write(b"test data")

        # Use select to check for readability
        rlist, _, _ = select.select([self.driver], [], [], 1)
        self.assertTrue(self.driver in rlist)

        # Read from the driver
        data = self.driver.read(9)
        self.assertEqual(data, b"test data")

    def test_write_to_driver(self):
        # Use select to check for writability
        _, wlist, _ = select.select([], [self.driver], [], 1)
        self.assertTrue(self.driver in wlist)

        # Write to the driver
        nbytes = self.driver.write(b"write test")
        self.assertEqual(nbytes, 10)
        self.assertEqual(self.driver.data, b"write test")

if __name__ == "__main__":
    unittest.main()
