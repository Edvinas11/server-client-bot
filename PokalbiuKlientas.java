import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.io.*;
import java.net.*;
import javax.swing.*;

public class PokalbiuKlientas {
    BufferedReader ivestis;
    PrintWriter isvestis;
    JFrame langas = new JFrame("pokalbiai");
    JTextField tekstoLaukelis = new JTextField(40);
    JTextArea pranesimuSritis = new JTextArea(8, 40);

    public PokalbiuKlientas() {
        tekstoLaukelis.setEditable(false);
        pranesimuSritis.setEditable(false);
        langas.getContentPane().add(tekstoLaukelis, "North");
        langas.getContentPane().add(new JScrollPane(pranesimuSritis), "Center");
        langas.pack();
        tekstoLaukelis.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent e) {
                isvestis.println(tekstoLaukelis.getText());
                tekstoLaukelis.setText("");
            }
        });
    }

    private String koksServerioAdresasIrPortas() {
        return JOptionPane.showInputDialog(langas, "Serverio IP ir portas?", "Klausimas", JOptionPane.QUESTION_MESSAGE);
    }

    private String koksVardas() {
        return JOptionPane.showInputDialog(langas, "Pasirink vardą:", "Vardas...", JOptionPane.PLAIN_MESSAGE);
    }

    private void run() throws IOException {
        String serverioAdresasIrPortas = koksServerioAdresasIrPortas();
        String[] info = serverioAdresasIrPortas.split("[ ]+");
        Socket soketas = new Socket(info[0], Integer.parseInt(info[1]));
        ivestis = new BufferedReader(new InputStreamReader(soketas.getInputStream()));
        isvestis = new PrintWriter(soketas.getOutputStream(), true);
        while (true) {
            String tekstas = ivestis.readLine();
            System.out.println("Serveris ---> Klientas : " + tekstas);
            if (tekstas.startsWith("ATSIUSKVARDA")) {
                isvestis.println(koksVardas());
            } else if (tekstas.startsWith("VARDASOK")) {
                tekstoLaukelis.setEditable(true);
            } else if (tekstas.startsWith("PRANESIMAS")) {
                pranesimuSritis.append(tekstas.substring(10) + "\n");
            }
        }
    }

    public static void main(String[] args) throws Exception {
        PokalbiuKlientas klientas = new PokalbiuKlientas();
        klientas.langas.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        klientas.langas.setVisible(true);
        klientas.run();
    }
}
